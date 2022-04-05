from flask import Flask
from flask import request

app = Flask(__name__)

@app.route('/mapreduce', methods = ['POST'])
def user():
    if request.method == 'POST':
        data = request.form
        print(data)