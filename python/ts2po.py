import xml.etree.ElementTree as ET
import polib


def main():
    file_path = "../translations/client_en.ts"
    file_path = "../translations/client_zh_TW.ts"


    po = polib.POFile()
    po.metadata = {
        'Project-Id-Version': '1.0',
        'Language': 'en',
    }

    # 讀取並解析 XML 檔案
    tree = ET.parse(file_path)
    root = tree.getroot()

    for message in root.findall(".//message"):
        location = message.find("location").attrib
        source = message.find("source").text
        translation_text = message.find("translation").text

        # 印出資訊
        # print(f"File: {location['filename']}, Line: {location['line']}")
        # print(f"Source: {source}")
        # print(f"Translation: {translation}")
        # print("------------")
        if translation_text is None:
            translation_text = ""

        entry = polib.POEntry(
            msgid=source,
            msgstr=translation_text,
            occurrences=[(location['filename'], location['line'])]
        )
        po.append(entry)

    po.save("../locales/zh_TW.po")


if __name__ == '__main__':
    main()
