#!/usr/bin/env python3
#/usr/bin/python3.8
import sys
def read_from_input(file):
    for line in file:
        yield line.strip().split(' ')



def main(separator = ' '):
    #print("Inmain")
    data = read_from_input(sys.stdin)
    for words in data:
        #print(words)
        for word in words:
            if word.isalnum():
            # write to the reduce
                print("{word}{separator}{count}".format(word=word, separator=' ', count=1))

if __name__ == '__main__':

    main()