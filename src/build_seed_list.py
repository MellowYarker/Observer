import requests
import json

print("Press ctrl + c to exit.\n")

url = "https://relatedwords.org/api/related?term={}"

try:
    with open("words.txt", "r") as f:
        print("Previously used words are stored in words.txt:")
        previous_words = f.readlines()
        for i in range(len(previous_words)):
            previous_words[i] = previous_words[i].rstrip()
            print('\t' + previous_words[i])
        print('')
except FileNotFoundError as e:
    previous_words = list()

while(True):
    word = input("Collect words related to: ")

    if (len(previous_words) > 0):
        if word in previous_words:
            print("This word has already been used.\n")
            continue

    r = requests.get(url.format(word))
    result = json.loads(r.text)

    with open("seeds.txt", "a") as f:
        for i in result:
            f.write(i['word'])
            f.write('\n')
        print(f"Found {len(result)} words related to '{word}''.")

    with open("words.txt", "a") as f:
        f.write(word)
        f.write('\n')
        print(f"Added '{word}' to words.txt")

    print(f"Words related to '{word}' have been added to seeds.txt\n")
    previous_words += word