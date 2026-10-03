import os
import canvasapi


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

# gather all current courses for the user, without including past courses
courses = result.get_courses(enrollment_state='active')

# gathering unsubmitted assignments to send to website
unsubmitted_list = []

# Looping through all courses
for course in courses:
    if not hasattr(course, 'name'):
        continue

    print(f"Course: {course.name} (ID: {course.id})")

    assignments = course.get_assignments(include=['submission']) # getting all current assignments
    has_unsubmitted_assignments = False

    for assignment in assignments:
        submission =getattr(assignment, 'submission', {})
    if isinstance(submission, dict):
            workflow_state = submission.get('workflow_state', 'unsubmitted')
    else:
                workflow_state = getattr(submission, 'workflow_state', 'unsubmitted')
    
    if workflow_state == 'unsubmitted': or workflow_state is None:
        has_unsubmitted_assignments = True

        payload = {
            "course_name" : course.name,
            "course_id" : course.id,
            "assignment_name" : assignment.name,
            "assignment_id" : assignment.id,
            "due_date" : getattr(assignment, 'due_at', "None")
        }

        unsubmitted_list.append(payload)
        


