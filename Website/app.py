import os
from flask import Flask, render_template, request, jsonify
from canvas_assignments import get_assignments
from datetime import datetime

app = Flask(__name__)

PORT = 3000
OUTPUT_FILE = os.path.join(os.path.dirname(__file__), "data.txt")


def clean(text):
    # Keep each entry on a single line
    return " ".join(str(text).split())

def pretty_date(iso):
    d = datetime.strptime(iso[:10], "%Y-%m-%d")
    return f"{d:%b} {d.day}"

@app.route("/")
def home():
    try:
        assignments = get_assignments()
    except Exception as error:
        print("Could not load Canvas assignments:", error)
        assignments = []

    return render_template("index.html", assignments=assignments)


@app.route("/save", methods=["POST"])
def save():
    data = request.get_json() or {}
    assignments = data.get("assignments", [])
    tasks = data.get("tasks", [])

    lines = []

    # Class: Assignment name; Due Date (soonest first)
    for a in sorted(assignments, key=lambda a: a["due_date"]):
        lines.append(
            f'{clean(a["course_name"])}: {clean(a["assignment_name"])}; {pretty_date(a["due_date"])}'
        )

    # *Task name
    for t in tasks:
        lines.append(f'*{clean(t["text"])}')

    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + ("\n" if lines else ""))

    return jsonify(ok=True)


if __name__ == "__main__":
    app.run(port=PORT, debug=True)