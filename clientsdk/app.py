from flask import Flask, request

app = Flask(__name__)

@app.route('/')
def my_app():
    return 'First Flask application!'


@app.route('/submitjob', methods= ['POST'])
def job_handler():
    print(request.json)
    return {}


if __name__ == '__main__':
   app.run(debug = True)