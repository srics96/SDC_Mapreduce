#!/usr/bin/env python3
#/usr/bin/python3.8
import sys
from itertools import groupby
from operator import itemgetter
from collections import defaultdict
 
def read_from_mapper(file, separator):
    for line in file:        
        a = line.strip().split(separator)
        if len(a) != 2:
            continue
        #print(a)
        yield a
        
 
def main(separator = ' '):
    data = read_from_mapper(sys.stdin, separator)
    dictionary = defaultdict(int)
    for k, v in data:
        try:
            dictionary[k] += int(v)
        except Exception:
            continue
        
    for k in sorted(dictionary.keys()):
        print ("{word}{separator}{count}".format(word=k, separator=' ', count=dictionary[k]))
 
if __name__ == '__main__':
    main()