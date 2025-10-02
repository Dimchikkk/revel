import sys

def generate_dsl(N):
    s = 20
    offset_x = 1000
    offset_y = 1000
    lines = []
    count = 0
    for i in range(N):
        for j in range(N):
            for k in range(N):
                x_pos = offset_x + i * s - k * s * 0.5
                y_pos = offset_y + j * s - k * s * 0.5
                r = int(i / (N-1) * 150) + 105 if N > 1 else 255
                g = int(j / (N-1) * 150) + 105 if N > 1 else 255
                b = int(k / (N-1) * 150) + 105 if N > 1 else 255
                lines.append(f'shape_create cube_{count} cube "" ({int(x_pos)},{int(y_pos)}) ({s},{s}) filled true bg #{r:02x}{g:02x}{b:02x} stroke 1 stroke_color #666666\n')
                count += 1
    return "".join(lines)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python ai_cube_generator.py <number_of_cubes_per_side>")
        sys.exit(1)
    try:
        N = int(sys.argv[1])
    except ValueError:
        print("Error: <number_of_cubes_per_side> must be an integer.")
        sys.exit(1)

    dsl_script = generate_dsl(N)
    with open('ai_cube.dsl', 'w') as f:
        f.write(dsl_script)
    print(f"Successfully generated ai_cube.dsl with {N*N*N} cubes.")
