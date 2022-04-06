#!/usr/bin/python3.8
import sys
import requests
from job_details import *
import json
import os, uuid
from azure.storage.blob import BlobServiceClient, BlobClient, ContainerClient, __version__

AZURE_STORAGE_CONNECTION_STRING ="DefaultEndpointsProtocol=https;AccountName=mrstoragesdc;AccountKey=nfiwDF+NaWkIZMK8XfZTMyGjUn7CJ8+A7SkB1Znl7+XQ94tBQPnhbAUCmUePct2sc6obSvDzXHJk+AStwFCY0g==;EndpointSuffix=core.windows.net"
AZURE_BLOB_CONTAINER = "map-reduce-container"
INPUT_FILE = "test_blob"

def main():
    try:
        print("Azure Blob Storage v" + __version__ + " - Python quickstart sample")
    except Exception as ex:
        print('Exception:')
        print(ex)
    blob_service_client = BlobServiceClient.from_connection_string(AZURE_STORAGE_CONNECTION_STRING)
    

    # Upload the created file
    for file in FILES:
        blob_client = blob_service_client.get_blob_client(container=AZURE_BLOB_CONTAINER, blob=file)

        print("\nUploading to Azure Storage as blob:\n\t" + file)
        with open(file, "rb") as data:
            blob_client.upload_blob(data)

    val = {'reducer_count': REDUCER_COUNT, 'files': FILES, 'shard_size': SHARD_SIZE}

    r = requests.post(JOB_URL, json=val)
    
    print(r.text)


if __name__ == '__main__':

    main()