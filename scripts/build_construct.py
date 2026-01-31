import os

OUTPUT_FILE = "BRAWLPIT_CONSTRUCT"

def main():
    build_num = os.environ.get('GITHUB_RUN_NUMBER', 'LOCAL')
    header = f"BRAWLPIT BUILD {build_num} CONSTRUCT"
    
    print(f"Generating {OUTPUT_FILE}...")
    
    with open(OUTPUT_FILE, "w", encoding="utf-8") as outfile:
        outfile.write(header + "\n")
        
        file_paths = []
        for root, dirs, files in os.walk("."):
            if "/." in root or root.startswith("./."):
                continue
            for f in files:
                path = os.path.join(root, f)
                clean_path = path.replace("./", "").replace("\\", "/")
                if clean_path == OUTPUT_FILE:
                    continue
                file_paths.append((clean_path, path))

        for clean_path, path in sorted(file_paths, key=lambda item: item[0]):
            try:
                with open(path, "r", encoding="utf-8") as infile:
                    content = infile.read()
                outfile.write(f"--- FILE START: {clean_path} ---\n")
                outfile.write(content)
                if not content.endswith("\n"):
                    outfile.write("\n")
                outfile.write(f"--- FILE END: {clean_path} ---\n")
            except Exception as e:
                print(f"Skipping {clean_path}: {e}")

    print("Done.")

if __name__ == "__main__":
    main()
