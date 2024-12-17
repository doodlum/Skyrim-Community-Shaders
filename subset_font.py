#!/usr/bin/env python3
import argparse
import subprocess
import sys
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
        description="Automate collecting unique characters and generate font subset."
    )
    parser.add_argument(
        "--input",
        type=str,
        default="https://github.com/adobe-fonts/source-han-sans/raw/release/Variable/OTC/SourceHanSans-VF.ttf.ttc",
        help="Path or URL to the original font file (e.g., C:/Windows/Fonts/msyh.ttc)."
    )
    parser.add_argument(
        "--output",
        type=str,
        default="package/Interface/CommunityShaders/Fonts/CommunityShaders.ttf",
        help="Path for the output subset font file (e.g., SubsetFont.ttf)."
    )
    parser.add_argument(
        "--text_dir",
        type=str,
        default="package/Interface/Translations",
        help="Directory to scan for translated files."
    )
    parser.add_argument(
        "--font_number",
        type=int,
        default=0,
        help="Font number of the font file (default: 0)."
    )
    return parser.parse_args()

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
    download_path = get_cache_dir() / Path(url).name
    if not download_path.is_file():
        print(f"Downloading font from '{url}' to '{download_path}'...")
        try:
            urlretrieve(url, download_path)
        except Exception as e:
            sys.exit(f"Error downloading font: {e}")
    return str(download_path)

def collect_unique_chars(input_dir, extension=".txt"):
    unique_chars = set()
    for file in Path(input_dir).rglob(f"*{extension}"):
        try:
            with file.open('r', encoding='utf-16-le', errors='ignore') as f:
                unique_chars.update(c for c in f.read() if c.isprintable())
        except Exception as e:
            print(f"Error reading '{file}': {e}", file=sys.stderr)
    return unique_chars

def subset_font(original_path, subset_path, text, font_number):
    try:
        options = subset.Options(font_number = font_number)
        subsetter = subset.Subsetter(options=options)
        subsetter.populate(text=text)

        with subset.load_font(str(original_path), options) as font:
            subsetter.subset(font)
            subset.save_font(font, str(subset_path), options)
        print(f"Subset font saved to '{subset_path}'.")
    except Exception as e:
        sys.exit(f"Error during font subsetting: {e}")

def main():
    args = parse_arguments()

    original_font = download_font(args.input) if is_url(args.input) else args.input
    if not Path(original_font).is_file():
        sys.exit(f"Error: Font file '{original_font}' does not exist.")

    unique_chars = collect_unique_chars(args.text_dir)
    if not unique_chars:
        sys.exit("No characters found. Exiting.")

    print(f"Collected {len(unique_chars)} unique characters.")

    subset_font(original_font, args.output, ''.join(unique_chars), args.font_number)

if __name__ == "__main__":
    main()
