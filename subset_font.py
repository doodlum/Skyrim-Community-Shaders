#!/usr/bin/env python3
import os
import argparse
import sys
import subprocess
from pathlib import Path
from urllib.parse import urlparse
from urllib.request import urlretrieve

try:
    from fontTools import subset
except ImportError:
    print("fonttools not found. Installing via pip...")
    subprocess.check_call([sys.executable, "-m", "pip", "install", "fonttools"])
    from fontTools import subset

def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Automate collecting unique characters and font tool."
    )
    parser.add_argument(
        "--input",
        type=str,
        required=False,
        default="https://github.com/adobe-fonts/source-han-sans/raw/release/Variable/OTC/SourceHanSans-VF.ttf.ttc",
        help="Path or URL to the original font file (e.g., C:/Windows/Fonts/msyh.ttc)."
    )
    parser.add_argument(
        "--output",
        type=str,
        required=False,
        default="package/Interface/CommunityShaders/Fonts/CommunityShaders.ttf",
        help="Path for the output subset font file (e.g., SubsetFont.ttf)."
    )
    parser.add_argument(
        "--text_dir",
        type=str,
        required=False,
        default="package/Interface/Translations",
        help="Directory to scan for translated files."
    )
    return parser.parse_args()

def is_printable(char):
    return char.isprintable()

def is_url(path):
    try:
        result = urlparse(path)
        return all([result.scheme, result.netloc])
    except ValueError:
        return False

def get_cache_dir():
    cache_path = Path.cwd() / "fonts"
    cache_path.mkdir(parents=True, exist_ok=True)
    return cache_path

def download_font(url):
    try:
        filename = url.split('/')[-1]
        download_path = get_cache_dir() / filename
        print(f"Downloading font from '{url}' to '{download_path}'...")
        urlretrieve(url, download_path)
        print(f"Downloaded font to '{download_path}'.")
        return str(download_path)
    except Exception as e:
        print(f"Error downloading font from '{url}': {e}", file=sys.stderr)
        sys.exit(1)

def collect_unique_chars(input_dir, extensions, exclude_dirs):
    unique_chars = set()
    input_path = Path(input_dir)
    if not input_path.is_dir():
        print(f"Error: Input directory '{input_dir}' does not exist or is not a directory.", file=sys.stderr)
        sys.exit(1)

    for root, dirs, files in os.walk(input_dir):
        dirs[:] = [d for d in dirs if d not in exclude_dirs]
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                file_path = Path(root) / file
                try:
                    with open(file_path, 'r', encoding='utf-16-le') as f:
                        content = f.read()
                        for char in content:
                            if is_printable(char):
                                unique_chars.add(char)
                except UnicodeDecodeError:
                    print(f"Warning: Could not decode '{file_path}'. Skipping.", file=sys.stderr)
                except Exception as e:
                    print(f"Error reading '{file_path}': {e}", file=sys.stderr)
    return unique_chars

def subset_font(original_path, subset_path, text):
    try:
        options = subset.Options(
            font_number = 0
        )

        subsetter = subset.Subsetter(options=options)
        subsetter.populate(text=text)

        font = subset.load_font(str(original_path), options)
        subsetter.subset(font)

        subset.save_font(font, str(subset_path), options)

        font.close()
    except Exception as e:
        print(f"Unexpected error during font subsetting: {e}", file=sys.stderr)
        sys.exit(1)

def main():
    args = parse_arguments()

    original_font = args.input
    output_font = args.output
    text_dir = args.text_dir

    if is_url(original_font):
        original_font = download_font(original_font)

    if not Path(original_font).is_file():
        print(f"Error: Font file '{original_font}' does not exist.", file=sys.stderr)
        sys.exit(1)

    print("Collecting unique characters from source files...")
    unique_chars = collect_unique_chars(text_dir, ".txt", [])
    num_chars = len(unique_chars)
    print(f"Collected {num_chars} unique characters.")

    text = ''.join(unique_chars)

    print("Generating subset font...")
    subset_font(original_font, output_font, text)

if __name__ == "__main__":
    main()
