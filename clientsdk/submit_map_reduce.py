#!/usr/bin/python3.8
import sys
import requests
from job_details import *
import json



def main():
    val = {'reducer_count': REDUCER_COUNT, 'files': FILES, 'shard_size': SHARD_SIZE}
    r = requests.post(JOB_URL, json=val)
    print(r.text)


if __name__ == '__main__':

    main()