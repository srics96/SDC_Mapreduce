#!/usr/bin/env python3
import sys
from itertools import groupby
from operator import itemgetter
from collections import defaultdict
 
def read_from_mapper(file, separator):
    for line in file:        
        a = line.strip().split(separator)
        #print(a)
        yield a
        
 
def main(separator = '\t'):
    data = read_from_mapper(sys.stdin, separator)
    dictionary = defaultdict(int)
    for k, v in data:
        dictionary[k] += int(v)
        
    for k in sorted(dictionary.keys()):
        print ("{word}{separator}{count}".format(word=k, separator='\t', count=dictionary[k]))
 
if __name__ == '__main__':
    main()