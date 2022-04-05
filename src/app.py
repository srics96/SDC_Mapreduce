from flask import Flask
from flask import request
from flask import jsonify
import subprocess

app = Flask(__name__)

@app.route('/mapreduce', methods = ['GET', 'POST'])
def user():
    if request.method == 'POST':
        data = request.form
        subprocess.Popen(['/code/src/master/master', data['shard_size'], data['file']], cwd="/", stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return jsonify({'message': 'Job submitted'})
