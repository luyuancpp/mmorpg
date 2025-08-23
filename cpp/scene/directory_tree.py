import os

def generate_directory_tree(start_path, file_out):
    with open(file_out, 'w', encoding='utf-8') as f:
        f.write(f"Directory Tree for {start_path}\n")
        f.write("================================================\n")

        for root, dirs, files in os.walk(start_path):
            level = root.replace(start_path, '').count(os.sep)
            indent = ' ' * 4 * level
            f.write(f"{indent}{os.path.basename(root)}/\n")
            sub_indent = ' ' * 4 * (level + 1)

            for file in files:
                f.write(f"{sub_indent}{file}\n")

generate_directory_tree('./', 'directory_tree.txt')
