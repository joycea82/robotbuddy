import os
import canvasapi
from datetime import datetime
from zoneinfo import ZoneInfo

TOKEN = os.environ.get('CANVAS_API_TOKEN')
BASEURL = 'https://canvas.sfu.ca'
#WEBSITE = ''

if not TOKEN:
    raise ValueError("Add the Canvas API token!")

# initializing the Canvas API object
canvas_api = canvasapi.Canvas(BASEURL, TOKEN)

# recording and printing account information
result = canvas_api.get_user('self')
print(f"User: {result.name}")
print(f"ID: {result.id}")

print ("\n)")

# gather all current courses for the user, without including past courses
courses = result.get_courses(enrollment_state='active')

# gathering unsubmitted assignments to send to website
unsubmitted_list = []

# Looping through all courses
for course in courses:
    if not hasattr(course, 'name'):
        continue

    print("\n")
    print(f"Course: {course.name} (ID: {course.id})")

    assignments = course.get_assignments(include=['submission']) # getting all current assignments
    has_unsubmitted_assignments = False

    for assignment in assignments:
        # checking for assignments without due dates, irrelevant
        if assignment.due_at is None:
            continue

        submission =getattr(assignment, 'submission', {})
        if isinstance(submission, dict):
            workflow_state = submission.get('workflow_state', 'unsubmitted')
        else:
                workflow_state = getattr(submission, 'workflow_state', 'unsubmitted')
    
        if workflow_state == 'unsubmitted' or workflow_state is None:
            has_unsubmitted_assignments = True

            due_date = datetime.fromisoformat(
            assignment.due_at.replace("Z", "+00:00")
            ).astimezone(ZoneInfo("America/Vancouver"))

            due_date = due_date.strftime("%a %b %d, %Y %I:%M%p")

            payload = {
                "course_name" : course.name,
                "course_id" : course.id,
                "assignment_name" : assignment.name,
                "assignment_id" : assignment.id,
                "due_date" : due_date
        }

            unsubmitted_list.append(payload)
            print(f" Unsubmitted: {assignment.name} (Due:{payload['due_date']})")

    if not has_unsubmitted_assignments:
         print("None")

print(f"\n--- Total Unsubmitted Collected: {len(unsubmitted_list)} ---")


