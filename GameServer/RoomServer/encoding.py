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


def should_convert(filepath):
    try:
        with open(filepath, "rb") as f:
            raw = f.read()

        # UTF-8 시도
        try:
            text_utf8 = raw.decode("utf-8")
            utf8_ok = True
        except:
            utf8_ok = False

        # EUC-KR 시도
        try:
            text_euc = raw.decode("euc-kr")
            euc_ok = True
        except:
            euc_ok = False

        # 핵심 조건
        if euc_ok and not utf8_ok and contains_korean(text_euc):
            return True

        return False

    except:
        return False


def convert_file(filepath):
    try:
        # 백업
        backup_path = filepath + ".bak"
        shutil.copy2(filepath, backup_path)

        # EUC-KR로 읽기
        with open(filepath, "r", encoding="euc-kr") as f:
            content = f.read()

        # UTF-8로 저장
        with open(filepath, "w", encoding="utf-8-sig") as f:
            f.write(content)

        print(f"[CONVERTED] {filepath}")

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