import os

OUTPUT_FILE = "BRAWLPIT_CONSTRUCT.txt"
DIRS_TO_SCAN = ["packages", "apps", "tests"]
EXTENSIONS = [".h", ".c", ".md"]

def main():
    build_num = os.environ.get('GITHUB_RUN_NUMBER', 'LOCAL')
    header = f"BRAWLPIT BUILD {build_num} CONSTRUCT"
    
    print(f"Generating {OUTPUT_FILE}...")
    
    with open(OUTPUT_FILE, "w", encoding="utf-8") as outfile:
        outfile.write(header + "\n")
        
        for root, dirs, files in os.walk("."):
            if "/." in root or root.startswith("./."): continue
            
            for f in files:
                if any(f.endswith(ext) for ext in EXTENSIONS):
                    path = os.path.join(root, f)
                    clean_path = path.replace("./", "").replace("\\", "/")
                    
                    if not any(clean_path.startswith(d) for d in DIRS_TO_SCAN) and f != "README.md":
                        continue
                        
                    try:
                        with open(path, "r", encoding="utf-8") as infile:
                            content = infile.read()
                        outfile.write(f"--- FILE START: {clean_path} ---\n")
                        outfile.write(content)
                        if not content.endswith("\n"): outfile.write("\n")
                        outfile.write(f"--- FILE END: {clean_path} ---\n")
                    except Exception as e:
                        print(f"Skipping {clean_path}: {e}")

    print("Done.")

if __name__ == "__main__":
    main()
