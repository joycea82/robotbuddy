import os
import canvasapi
from datetime import datetime
from zoneinfo import ZoneInfo
from dotenv import load_dotenv

load_dotenv()

def get_assignments():

    TOKEN = os.environ.get('CANVAS_API_TOKEN')
    BASEURL = 'https://canvas.sfu.ca'

    if not TOKEN:
        raise ValueError("Add the Canvas API token!")

    # Initialize Canvas API
    canvas_api = canvasapi.Canvas(BASEURL, TOKEN)

    # Get the current user
    result = canvas_api.get_user('self')

    # Get current courses
    courses = result.get_courses(enrollment_state='active')

    # List that will contain all unsubmitted assignments
    unsubmitted_list = []

    # Loop through all courses
    for course in courses:

        if not hasattr(course, 'name'):
            continue

        assignments = course.get_assignments(include=['submission'])

        # Loop through assignments
        for assignment in assignments:

            # Ignore assignments without due dates
            if assignment.due_at is None:
                continue

            # Get submission information
            submission = getattr(assignment, 'submission', {})

            if isinstance(submission, dict):
                workflow_state = submission.get(
                    'workflow_state',
                    'unsubmitted'
                )
            else:
                workflow_state = getattr(
                    submission,
                    'workflow_state',
                    'unsubmitted'
                )

            # Only include unsubmitted assignments
            if workflow_state == 'unsubmitted' or workflow_state is None:

                # Convert Canvas date to Vancouver time
                due_date = datetime.fromisoformat(
                    assignment.due_at.replace("Z", "+00:00")
                ).astimezone(
                    ZoneInfo("America/Vancouver")
                )

                # Store assignment information
                payload = {
                    "course_name": course.name,
                    "course_id": course.id,
                    "assignment_name": assignment.name,
                    "assignment_id": assignment.id,
                    "due_date": due_date.strftime("%Y-%m-%d"),
                    "due_time": due_date.strftime("%I:%M %p")
                }

                unsubmitted_list.append(payload)

    return unsubmitted_list

assignments = get_assignments()

for assignment in assignments:
    print(assignment)