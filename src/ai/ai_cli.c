#include "ai_cli.h"

#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <gio/gio.h>

#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__)
#include <util.h>
#endif

#define AI_CLI_DEFAULT_TIMEOUT_MS 60000
#define AI_CLI_READ_CHUNK 4096

static void ai_cli_debug_write(const gchar *env_key,
                               const gchar *default_path,
                               const gchar *content) {
  if (!content) {
    return;
  }

  const gchar *path = g_getenv(env_key);
  if (!path || !*path) {
    const gchar *toggle = g_getenv("REVEL_AI_DEBUG");
    if (!toggle || !*toggle) {
      return;
    }
    path = default_path;
  }

  GError *error = NULL;
  g_file_set_contents(path, content, -1, &error);
  if (error) {
    g_error_free(error);
  }
}

static void strip_ansi_sequences(char *text) {
  if (!text) {
    return;
  }

  char *src = text;
  char *dst = text;
  while (*src) {
    if (*src == '\x1b') {
      src++;
      if (*src == '[') {
        src++;
        while (*src && !(*src >= '@' && *src <= '~')) {
          src++;
        }
        if (*src) {
          src++;
        }
        continue;
      }
      if (*src == ']') {
        src++;
        while (*src && *src != '\a') {
          src++;
        }
        if (*src == '\a') {
          src++;
        }
        continue;
      }
      while (*src && !(*src >= '@' && *src <= '~')) {
        src++;
      }
      if (*src) {
        src++;
      }
      continue;
    }
    *dst++ = *src++;
  }
  *dst = '\0';
}

static gboolean looks_like_dsl_line(const gchar *line) {
  if (!line || *line == '\0') {
    return FALSE;
  }

  static const gchar * const prefixes[] = {
    "shape_create",
    "note_create",
    "paper_note_create",
    "text_create",
    "text_update",
    "note_update",
    "paper_note_update",
    "space_",
    "element_",
    "image_create",
    "video_create",
    "audio_create",
    "media_create",
    "connect",
    "disconnect",
    "animate_",
    "for ",
    "end",
    "set ",
    "on ",
    "off ",
    "wait ",
    "background_",
    "dsl_version",
    "load_space",
    "save_space",
    "clone_",
    "group_",
    "ungroup",
    "tag_",
    "untag",
    "delete",
    "update ",
    "move ",
    "resize ",
    "rotate ",
    "color ",
    "audio_",
    "video_",
    "image_",
    "path_"
  };

  for (guint i = 0; i < G_N_ELEMENTS(prefixes); i++) {
    if (g_str_has_prefix(line, prefixes[i])) {
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean args_contains(const gchar * const *args, const gchar *value) {
  if (!args || !value) {
    return FALSE;
  }
  for (guint i = 0; args[i]; i++) {
    if (g_strcmp0(args[i], value) == 0) {
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean line_is_timestamp(const gchar *line) {
  if (!line || line[0] != '[') {
    return FALSE;
  }
  return strchr(line, ']') != NULL;
}

static gchar *extract_codex_segment(gchar **lines) {
  if (!lines) {
    return NULL;
  }

  gboolean capture = FALSE;
  GString *buffer = NULL;

  for (guint i = 0; lines[i]; i++) {
    gchar *line = lines[i];
    if (!capture) {
      if (g_str_has_prefix(line, "[") && strstr(line, "] codex")) {
        capture = TRUE;
        if (!buffer) {
          buffer = g_string_new(NULL);
        }
        continue;
      }
      continue;
    }

    if (line_is_timestamp(line) || g_str_has_prefix(line, "tokens used") ||
        g_str_has_prefix(line, "[tokens used")) {
      break;
    }

    gchar *trimmed = g_strstrip(line);
    if (buffer->len == 0 && *trimmed == '\0') {
      continue;
    }
    if (buffer->len > 0) {
      g_string_append_c(buffer, '\n');
    }
    g_string_append(buffer, trimmed);
  }

  if (!buffer || buffer->len == 0) {
    if (buffer) {
      g_string_free(buffer, TRUE);
    }
    return NULL;
  }

  return g_string_free(buffer, FALSE);
}

static gchar *extract_code_block(const gchar *text) {
  if (!text) {
    return NULL;
  }

  const gchar *search = text;
  const gchar *best_start = NULL;
  const gchar *best_end = NULL;

  while ((search = strstr(search, "```") ) != NULL) {
    const gchar *block_start = search + 3;
    while (*block_start && *block_start != '\n') {
      block_start++;
    }
    if (*block_start == '\n') {
      block_start++;
    }
    const gchar *block_end = strstr(block_start, "```");
    if (!block_end) {
      break;
    }
    best_start = block_start;
    best_end = block_end;
    search = block_end + 3;
  }

  if (best_start && best_end && best_end > best_start) {
    return g_strndup(best_start, best_end - best_start);
  }

  return NULL;
}

static gchar *strip_leading(const gchar *line) {
  if (!line) {
    return NULL;
  }

  const gchar *cursor = line;
  if (*cursor == '-' || *cursor == '*') {
    cursor++;
    while (*cursor && g_ascii_isspace(*cursor)) {
      cursor++;
    }
  }
  if (g_ascii_isdigit(*cursor)) {
    const gchar *digits = cursor;
    while (g_ascii_isdigit(*digits)) {
      digits++;
    }
    if (*digits == '.' || *digits == ')') {
      cursor = digits + 1;
      while (*cursor && g_ascii_isspace(*cursor)) {
        cursor++;
      }
    }
  }
  if (g_str_has_prefix(cursor, "DSL:")) {
    cursor += 4;
    while (*cursor && g_ascii_isspace(*cursor)) {
      cursor++;
    }
  }
  return g_strdup(cursor);
}

static gchar *ai_cli_normalize_output(char *raw) {
  if (!raw) {
    return NULL;
  }

  gchar *text = g_strstrip(raw);

  // Strip carriage returns
  gchar *dst = text;
  for (gchar *src = text; *src; src++) {
    if (*src == '\r') {
      continue;
    }
    *dst++ = *src;
  }
  *dst = '\0';

  strip_ansi_sequences(text);
  ai_cli_debug_write("REVEL_AI_DEBUG_STDOUT_SANITIZED", "/tmp/ai_stdout_sanitized.txt", text);

  gchar **lines = g_strsplit(text, "\n", 0);
  gchar *codex_dsl = extract_codex_segment(lines);
  if (codex_dsl) {
    gchar *stripped = g_strstrip(codex_dsl);
    g_strfreev(lines);
    if (*stripped == '\0') {
      g_free(codex_dsl);
      g_free(raw);
      return NULL;
    }
    gchar *result = g_strdup(stripped);
    g_free(codex_dsl);
    g_free(raw);
    return result;
  }
  g_strfreev(lines);

  gchar *code = extract_code_block(text);
  if (code) {
    gchar *stripped = g_strstrip(code);
    if (strstr(stripped, "::=") == NULL &&
        !g_str_has_prefix(stripped, "COMMENT ") &&
        !g_str_has_prefix(stripped, "Program ::=")) {
      g_free(raw);
      gchar *normalized = g_strdup(stripped);
      g_free(code);
      return normalized;
    }
    g_free(code);
  }

  lines = g_strsplit(text, "\n", 0);
  GString *result = g_string_new(NULL);

  for (guint i = 0; lines && lines[i]; i++) {
    gchar *line_trim = g_strstrip(lines[i]);
    if (*line_trim == '\0') {
      continue;
    }
    if (g_str_has_prefix(line_trim, "AI:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "Attempt")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "Retry")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "Here")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "Explanation")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "###")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "```") || g_strcmp0(line_trim, "```") == 0) {
      continue;
    }
    if (strstr(line_trim, "::=") != NULL) {
      continue;
    }
    if (line_trim[0] == '[' && strchr(line_trim, ']')) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "model:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "provider:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "workdir:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "approval:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "sandbox:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "reasoning")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "User instructions:")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "tokens used")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "--------")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "##")) {
      continue;
    }
    if (g_str_has_prefix(line_trim, "--")) {
      continue;
    }

    gchar *clean_line = strip_leading(line_trim);
    gchar *clean_trim = clean_line ? g_strstrip(clean_line) : NULL;
    if (clean_trim && *clean_trim != '\0' && looks_like_dsl_line(clean_trim)) {
      if (result->len > 0) {
        g_string_append_c(result, '\n');
      }
      g_string_append(result, clean_trim);
    }
    g_free(clean_line);
  }

  g_strfreev(lines);
  if (result->len == 0) {
    g_string_free(result, TRUE);
    g_free(raw);
    return NULL;
  }

  gchar *dsl = g_string_free(result, FALSE);
  g_free(raw);
  gchar *trimmed = g_strstrip(dsl);
  if (*trimmed == '\0') {
    g_free(dsl);
    return NULL;
  }
  return trimmed;
}

static gchar **build_argv(const AiProvider *provider, const gchar *payload, gboolean *out_use_stdin) {
  gboolean use_stdin = ai_provider_get_payload_mode(provider) == AI_PAYLOAD_STDIN;
  const gchar *stdin_flag = ai_provider_get_stdin_flag(provider);

  guint arg_count = 1; // binary
  if (provider->default_args) {
    while (provider->default_args[arg_count - 1]) {
      arg_count++;
    }
  }

  gboolean need_input_flag = use_stdin && stdin_flag && !args_contains((const gchar * const *)provider->default_args, stdin_flag);
  if (need_input_flag) {
    arg_count++;
  }

  gboolean append_flag = FALSE;
  const gchar *flag = ai_provider_get_arg_flag(provider);
  if (!use_stdin && flag && *flag) {
    append_flag = TRUE;
    arg_count++;
  }

  gboolean append_payload = !use_stdin && payload && *payload;
  if (append_payload) {
    arg_count++;
  }

  gchar **argv = g_new0(gchar *, arg_count + 1);
  guint index = 0;
  argv[index++] = g_strdup(provider->binary);
  if (provider->default_args) {
    for (guint i = 0; provider->default_args[i]; i++) {
      argv[index++] = g_strdup(provider->default_args[i]);
    }
  }
  if (need_input_flag) {
    argv[index++] = g_strdup(stdin_flag);
  }
  if (append_flag) {
    argv[index++] = g_strdup(flag);
  }
  if (append_payload) {
    argv[index++] = g_strdup(payload);
  }
  argv[index] = NULL;

  if (out_use_stdin) {
    *out_use_stdin = use_stdin && payload && *payload;
  }
  return argv;
}

static void free_argv(gchar **argv) {
  if (!argv) {
    return;
  }
  g_strfreev(argv);
}

static gboolean ensure_binary_available(const gchar *binary, gchar **out_error) {
  gchar *resolved = g_find_program_in_path(binary);
  if (!resolved) {
    if (out_error) {
      *out_error = g_strdup_printf("Provider binary '%s' not found in PATH", binary);
    }
    return FALSE;
  }
  g_free(resolved);
  return TRUE;
}

static gboolean write_payload(int fd, const gchar *payload, GCancellable *cancellable, gchar **out_error) {
  if (fd < 0 || !payload) {
    return TRUE;
  }

  const gchar *cursor = payload;
  gsize remaining = strlen(payload);
  while (remaining > 0) {
    if (cancellable && g_cancellable_is_cancelled(cancellable)) {
      if (out_error && !*out_error) {
        *out_error = g_strdup("Request cancelled");
      }
      return FALSE;
    }
    ssize_t written = write(fd, cursor, remaining);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (out_error) {
        *out_error = g_strdup_printf("Failed to write to provider stdin: %s", g_strerror(errno));
      }
      return FALSE;
    }
    cursor += written;
    remaining -= written;
  }
  return TRUE;
}

static void close_fd(int *fd) {
  if (fd && *fd >= 0) {
    close(*fd);
    *fd = -1;
  }
}

#if defined(__linux__) || defined(__APPLE__)
static gboolean spawn_with_pty(char **argv,
                               gboolean need_stdin,
                               GPid *pid_out,
                               gint *stdin_fd_out,
                               gint *stdout_fd_out,
                               gint *stderr_fd_out,
                               gchar **out_error) {
  int master_fd = -1;
  pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);
  if (pid < 0) {
    if (out_error) {
      *out_error = g_strdup_printf("forkpty failed: %s", g_strerror(errno));
    }
    return FALSE;
  }

  if (pid == 0) {
    execvp(argv[0], argv);
    _exit(127);
  }

  if (stdout_fd_out) {
    *stdout_fd_out = master_fd;
  } else {
    close(master_fd);
  }

  if (stderr_fd_out) {
    *stderr_fd_out = -1;
  }

  if (need_stdin && stdin_fd_out) {
    int dup_fd = dup(master_fd);
    if (dup_fd < 0) {
      int status = 0;
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      if (stdout_fd_out && *stdout_fd_out >= 0) {
        close_fd(stdout_fd_out);
      }
      if (out_error) {
        *out_error = g_strdup_printf("dup failed: %s", g_strerror(errno));
      }
      return FALSE;
    }
    *stdin_fd_out = dup_fd;
  } else if (stdin_fd_out) {
    *stdin_fd_out = -1;
  }

  if (pid_out) {
    *pid_out = pid;
  }

  return TRUE;
}
#else
static gboolean spawn_with_pty(char **argv,
                               gboolean need_stdin,
                               GPid *pid_out,
                               gint *stdin_fd_out,
                               gint *stdout_fd_out,
                               gint *stderr_fd_out,
                               gchar **out_error) {
  (void)argv;
  (void)need_stdin;
  (void)pid_out;
  (void)stdin_fd_out;
  (void)stdout_fd_out;
  (void)stderr_fd_out;
  if (out_error) {
    *out_error = g_strdup("PTY spawning not supported on this platform");
  }
  return FALSE;
}
#endif

static gboolean drain_fd(int *fd_ptr, GString *buffer, GCancellable *cancellable, gchar **out_error) {
  if (!fd_ptr || *fd_ptr < 0) {
    return TRUE;
  }

  char chunk[AI_CLI_READ_CHUNK];
  while (TRUE) {
    if (cancellable && g_cancellable_is_cancelled(cancellable)) {
      return FALSE;
    }
    ssize_t bytes = read(*fd_ptr, chunk, sizeof(chunk));
    if (bytes > 0) {
      g_string_append_len(buffer, chunk, bytes);
      if (bytes < (ssize_t)sizeof(chunk)) {
        return TRUE;
      }
      continue;
    }
    if (bytes == 0) {
      close_fd(fd_ptr);
      return TRUE;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EIO) {
      close_fd(fd_ptr);
      return TRUE;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return TRUE;
    }
    if (out_error && !*out_error) {
      *out_error = g_strdup_printf("Read failed: %s", g_strerror(errno));
    }
    close_fd(fd_ptr);
    return FALSE;
  }
}

static void terminate_child(GPid pid) {
  if (pid <= 0) {
    return;
  }
  kill(pid, SIGTERM);
  gint64 deadline = g_get_monotonic_time() + (gint64)2 * G_TIME_SPAN_SECOND;
  int status = 0;
  while (g_get_monotonic_time() < deadline) {
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      g_spawn_close_pid(pid);
      return;
    }
    g_usleep(50000);
  }
  kill(pid, SIGKILL);
  waitpid(pid, &status, 0);
  g_spawn_close_pid(pid);
}

gboolean ai_cli_generate_with_timeout(const AiProvider *provider, const gchar *payload, guint timeout_ms, GCancellable *cancellable, gchar **out_dsl, gchar **out_error) {
  if (out_dsl) {
    *out_dsl = NULL;
  }
  if (out_error) {
    *out_error = NULL;
  }

  if (!provider || !provider->binary) {
    if (out_error) {
      *out_error = g_strdup("Invalid AI provider definition");
    }
    return FALSE;
  }

  if (!ensure_binary_available(provider->binary, out_error)) {
    return FALSE;
  }

  gboolean need_stdin = FALSE;
  gboolean uses_pty = ai_provider_requires_pty(provider);
  gchar **argv = build_argv(provider, payload, &need_stdin);

  GPid pid = 0;
  gint stdin_fd = -1;
  gint stdout_fd = -1;
  gint stderr_fd = -1;
  GError *spawn_error = NULL;

  gboolean spawned = FALSE;

  if (uses_pty) {
    gchar *spawn_error_msg = NULL;
    spawned = spawn_with_pty(argv,
                             need_stdin,
                             &pid,
                             need_stdin ? &stdin_fd : NULL,
                             &stdout_fd,
                             &stderr_fd,
                             &spawn_error_msg);
    if (!spawned) {
      if (out_error) {
        *out_error = spawn_error_msg ? spawn_error_msg : g_strdup("Failed to spawn provider");
      } else {
        g_free(spawn_error_msg);
      }
      free_argv(argv);
      return FALSE;
    }
    g_free(spawn_error_msg);
  } else {
    spawned = g_spawn_async_with_pipes(
      NULL,
      argv,
      NULL,
      G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
      NULL,
      NULL,
      &pid,
      need_stdin ? &stdin_fd : NULL,
      &stdout_fd,
      &stderr_fd,
      &spawn_error
    );
  }

  free_argv(argv);

  if (!spawned) {
    if (!uses_pty) {
      if (out_error) {
        *out_error = g_strdup(spawn_error ? spawn_error->message : "Failed to spawn provider");
      }
      if (spawn_error) {
        g_error_free(spawn_error);
      }
    }
    return FALSE;
  }

  if (spawn_error) {
    g_error_free(spawn_error);
  }

  gboolean success = TRUE;
  gboolean timed_out = FALSE;
  GString *stdout_buffer = g_string_new(NULL);
  GString *stderr_buffer = g_string_new(NULL);

  if (need_stdin) {
    if (!write_payload(stdin_fd, payload, cancellable, out_error)) {
      success = FALSE;
    }
  }
  close_fd(&stdin_fd);

  gboolean watch_stdout = stdout_fd >= 0;
  gboolean watch_stderr = stderr_fd >= 0;

  gint64 deadline = g_get_monotonic_time() + (gint64)timeout_ms * G_TIME_SPAN_MILLISECOND;
  int exit_status = 0;
  gboolean child_exited = FALSE;

  while (success && (!child_exited || watch_stdout || watch_stderr)) {
    gint timeout_poll = -1;
    if (timeout_ms > 0) {
      gint64 now = g_get_monotonic_time();
      if (now >= deadline) {
        timed_out = TRUE;
        success = FALSE;
        break;
      }
      timeout_poll = (gint)((deadline - now) / 1000);
      if (timeout_poll < 0) {
        timeout_poll = 0;
      }
    }

    struct pollfd fds[2];
    int nfds = 0;
    if (watch_stdout) {
      fds[nfds].fd = stdout_fd;
      fds[nfds].events = POLLIN | POLLHUP | POLLERR;
      fds[nfds].revents = 0;
      nfds++;
    }
    if (watch_stderr) {
      fds[nfds].fd = stderr_fd;
      fds[nfds].events = POLLIN | POLLHUP | POLLERR;
      fds[nfds].revents = 0;
      nfds++;
    }

    int poll_result = nfds > 0 ? poll(fds, nfds, timeout_poll) : 0;
    if (poll_result < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (out_error) {
        *out_error = g_strdup_printf("Poll failed: %s", g_strerror(errno));
      }
      success = FALSE;
      break;
    }

    if (poll_result > 0) {
      int idx = 0;
      if (watch_stdout) {
        if (fds[idx].revents & (POLLIN | POLLHUP)) {
          if (!drain_fd(&stdout_fd, stdout_buffer, cancellable, out_error)) {
            success = FALSE;
          }
          watch_stdout = stdout_fd >= 0;
        } else if (fds[idx].revents & POLLERR) {
          if (out_error && !*out_error) {
            *out_error = g_strdup("stdout stream error");
          }
          close_fd(&stdout_fd);
          watch_stdout = FALSE;
          success = FALSE;
        }
        idx++;
      }
      if (success && watch_stderr) {
        if (fds[idx].revents & (POLLIN | POLLHUP)) {
          if (!drain_fd(&stderr_fd, stderr_buffer, cancellable, out_error)) {
            success = FALSE;
          }
          watch_stderr = stderr_fd >= 0;
        } else if (fds[idx].revents & POLLERR) {
          if (out_error && !*out_error) {
            *out_error = g_strdup("stderr stream error");
          }
          close_fd(&stderr_fd);
          watch_stderr = FALSE;
        }
      }
    }

    if (!success) {
      break;
    }

    if (cancellable && g_cancellable_is_cancelled(cancellable)) {
      success = FALSE;
      break;
    }

    if (!child_exited) {
      pid_t wait_result = waitpid(pid, &exit_status, WNOHANG);
      if (wait_result == pid) {
        child_exited = TRUE;
      } else if (wait_result < 0 && errno != EINTR) {
        success = FALSE;
        if (out_error) {
          *out_error = g_strdup_printf("waitpid failed: %s", g_strerror(errno));
        }
        break;
      }
    }

    if (nfds == 0 && !child_exited) {
      g_usleep(50000);
    }
  }

  close_fd(&stdout_fd);
  close_fd(&stderr_fd);

  if (success && !child_exited) {
    timed_out = TRUE;
    success = FALSE;
  }

  if (timed_out) {
    terminate_child(pid);
    if (out_error) {
      *out_error = g_strdup("AI provider timed out");
    }
  } else if (!success) {
    terminate_child(pid);
    if (out_error && !*out_error && cancellable && g_cancellable_is_cancelled(cancellable)) {
      *out_error = g_strdup("Request cancelled");
    }
  } else {
    g_spawn_close_pid(pid);
  }

  if (!success) {
    ai_cli_debug_write("REVEL_AI_DEBUG_STDOUT", "/tmp/ai_stdout.txt", stdout_buffer->len ? stdout_buffer->str : NULL);
    ai_cli_debug_write("REVEL_AI_DEBUG_STDERR", "/tmp/ai_stderr.txt", stderr_buffer->len ? stderr_buffer->str : NULL);
    const gchar *fallback = NULL;
    if (stderr_buffer->len > 0) {
      fallback = stderr_buffer->str;
    } else if (uses_pty && stdout_buffer->len > 0) {
      fallback = stdout_buffer->str;
    }
    g_string_free(stdout_buffer, TRUE);
    if (out_error && !*out_error) {
      *out_error = g_strdup(fallback ? fallback : "unknown error");
    }
    g_string_free(stderr_buffer, TRUE);
    return FALSE;
  }

  if (!WIFEXITED(exit_status) || WEXITSTATUS(exit_status) != 0) {
    if (out_error) {
      const gchar *stderr_text = stderr_buffer->len ? stderr_buffer->str : "provider exited with error";
      *out_error = g_strdup_printf("Provider failed (status %d): %s", WEXITSTATUS(exit_status), stderr_text);
    }
    g_string_free(stdout_buffer, TRUE);
    g_string_free(stderr_buffer, TRUE);
    return FALSE;
  }

  if (out_dsl) {
    char *raw_output = g_string_free(stdout_buffer, FALSE);
    char *raw_copy = g_strdup(raw_output ? raw_output : "");
    ai_cli_debug_write("REVEL_AI_DEBUG_STDOUT", "/tmp/ai_stdout.txt", raw_copy);
    char *normalized = ai_cli_normalize_output(raw_output);
    if (!normalized) {
      ai_cli_debug_write("REVEL_AI_DEBUG_STDERR", "/tmp/ai_stderr.txt", stderr_buffer->len ? stderr_buffer->str : NULL);
      g_string_free(stderr_buffer, TRUE);
      if (out_error) {
        if (raw_copy && *raw_copy) {
          *out_error = g_strdup_printf("AI provider did not return DSL content. Raw response:\n%s", raw_copy);
        } else {
          *out_error = g_strdup("AI provider did not return DSL content");
        }
      }
      g_free(raw_copy);
      return FALSE;
    }
    *out_dsl = normalized;
    g_free(raw_copy);
  } else {
    g_string_free(stdout_buffer, TRUE);
  }

  ai_cli_debug_write("REVEL_AI_DEBUG_STDERR", "/tmp/ai_stderr.txt", stderr_buffer->len ? stderr_buffer->str : NULL);

  if (stderr_buffer->len > 0 && out_error && !*out_error) {
    *out_error = g_string_free(stderr_buffer, FALSE);
  } else {
    g_string_free(stderr_buffer, TRUE);
  }

  return TRUE;
}

gboolean ai_cli_generate(const AiProvider *provider, const gchar *payload, GCancellable *cancellable, gchar **out_dsl, gchar **out_error) {
  return ai_cli_generate_with_timeout(provider, payload, AI_CLI_DEFAULT_TIMEOUT_MS, cancellable, out_dsl, out_error);
}
