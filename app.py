from flask import Flask, render_template
from canvas_assignments import get_assignments

app = Flask(__name__)

PORT = 3000

@app.route("/")
def home():
    assignments = get_assignments()

    return render_template("index.html", assignments=assignments)


if __name__ == "__main__":
    app.run(port=PORT, debug=True)