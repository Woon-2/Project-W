import os
import shutil

TARGET_EXTENSIONS = (".hpp", ".cpp")

def is_valid_encoding(filepath, encoding):
    try:
        with open(filepath, "r", encoding=encoding) as f:
            f.read()
        return True
    except:
        return False


def contains_korean(text):
    for ch in text:
        if '\uac00' <= ch <= '\ud7a3':
            return True
    return False


def has_bom(raw):
    return raw.startswith(b'\xef\xbb\xbf')


def should_convert(filepath):
    try:
        with open(filepath, "rb") as f:
            raw = f.read()

        # BOM 여부
        bom = has_bom(raw)

        # UTF-8 체크
        try:
            text_utf8 = raw.decode("utf-8")
            utf8_ok = True
        except:
            utf8_ok = False

        # cp949 체크
        try:
            text_euc = raw.decode("cp949")
            euc_ok = True
        except:
            euc_ok = False

        # cp949 → 변환
        if euc_ok and not utf8_ok and contains_korean(text_euc):
            return True

        # UTF-8인데 BOM 없음 → 변환
        if utf8_ok and not bom:
            return True

        return False

    except:
        return False


def convert_file(filepath):
    try:
        with open(filepath, "rb") as f:
            raw = f.read()

        # UTF-8 시도
        try:
            text = raw.decode("utf-8")
            source = "utf-8"
        except:
            # CP949 시도
            try:
                text = raw.decode("cp949")
                source = "cp949"
            except:
                print(f"[SKIP - UNKNOWN ENCODING] {filepath}")
                return

        # BOM 포함 UTF-8로 저장
        with open(filepath, "w", encoding="utf-8-sig", newline="") as f:
            f.write(text)

        print(f"[CONVERTED {source}] {filepath}")

    except Exception as e:
        print(f"[FAIL] {filepath} -> {e}")


def main():
    converted = 0
    skipped = 0

    for root, _, files in os.walk("."):
        for file in files:
            if file.endswith(TARGET_EXTENSIONS):
                path = os.path.join(root, file)

                if should_convert(path):
                    convert_file(path)
                    converted += 1
                else:
                    print(f"[SKIPPED] {path}")
                    skipped += 1

    print("\n=== RESULT ===")
    print(f"Converted: {converted}")
    print(f"Skipped: {skipped}")


if __name__ == "__main__":
    main()