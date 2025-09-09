# Velo2

Note taking brainstorming app

# Features:

* Create notes and paper notes.
* Connect elements with arrows.
* Organize elements into nested spaces.
* Paste images from the clipboard and annotate them.
* Store all data in a single portable SQLite3 database file.
* Resize, move, fork, clone (by text or by size), and delete elements.
* Navigate spaces hierarchically.

## Dependencies

GTK4, sqlite3

```
sudo apt install libgtk-4-dev
sudo apt install libsqlite3-dev
```

## To run on x86_64 GNU/Linux:

`make -B -j 7 && ./velo2`
