const addTaskButton = document.getElementById("addTaskButton");
const todoList = document.getElementById("todoList");

const calendar = document.getElementById("calendar");
const monthName = document.getElementById("monthName");
const previousMonth = document.getElementById("previousMonth");
const nextMonth = document.getElementById("nextMonth");


// -------------------------
// STORAGE HELPERS
// -------------------------

function loadData(key, fallback) {
    try {
        const value = JSON.parse(localStorage.getItem(key));
        return value === null ? fallback : value;
    } catch (error) {
        return fallback;
    }
}

function saveData(key, value) {
    try {
        localStorage.setItem(key, JSON.stringify(value));
    } catch (error) {
        console.error("Could not save:", error);
    }

    syncToFile();
}

function syncToFile() {

    // Start of today, and the end of the 7-day window
    const today = new Date();
    today.setHours(0, 0, 0, 0);

    const weekFromToday = new Date(today);
    weekFromToday.setDate(weekFromToday.getDate() + 7);

    // Only assignments that haven't been deleted AND are due within the next week
    const upcomingAssignments = allAssignments.filter(function(assignment) {

        if (hiddenAssignments.includes(assignment.assignment_id)) {
            return false;
        }

        const dueDate = new Date(assignment.due_date + "T00:00:00");

        return dueDate >= today && dueDate <= weekFromToday;
    });

    // Only tasks that aren't checked off
    const pendingTasks = tasks.filter(function(task) {
        return !task.done;
    });

    fetch("/save", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
            assignments: upcomingAssignments,
            tasks: pendingTasks
        })
    }).catch(function(error) {
        console.error("Could not write planner.txt:", error);
    });

}


// -------------------------
// DATA
// -------------------------

// Assignments from Canvas (passed in by Flask)
let allAssignments = [];

try {
    allAssignments = JSON.parse(document.body.dataset.assignments || "[]");
} catch (error) {
    console.error("Could not read assignments:", error);
}

// Assignments the student deleted (saved by assignment_id)
let hiddenAssignments = loadData("hiddenAssignments", []);

// To-do tasks: { id, text, done }
let tasks = loadData("tasks", []);


// -------------------------
// TO-DO LIST
// -------------------------

function createTodoList() {

    todoList.innerHTML = "";

    tasks.forEach(function(task) {

        const todo = document.createElement("div");
        todo.classList.add("todo");

        const checkbox = document.createElement("input");
        checkbox.type = "checkbox";
        checkbox.checked = task.done;

        checkbox.addEventListener("change", function() {
            task.done = checkbox.checked;
            saveData("tasks", tasks);
        });

        const text = document.createElement("span");
        text.textContent = task.text;

        const deleteButton = document.createElement("button");
        deleteButton.classList.add("delete-button");
        deleteButton.textContent = "×";
        deleteButton.title = "Delete task";

        deleteButton.addEventListener("click", function() {
            tasks = tasks.filter(function(t) {
                return t.id !== task.id;
            });
            saveData("tasks", tasks);
            createTodoList();
        });

        todo.appendChild(checkbox);
        todo.appendChild(text);
        todo.appendChild(deleteButton);

        todoList.appendChild(todo);

    });

}

addTaskButton.addEventListener("click", function() {

    const taskText = prompt("What do you need to do?");

    if (taskText === null || taskText.trim() === "") {
        return;
    }

    tasks.push({
        id: Date.now(),
        text: taskText.trim(),
        done: false
    });

    saveData("tasks", tasks);
    createTodoList();

});


// -------------------------
// CALENDAR
// -------------------------

let currentDate = new Date();

let currentMonth = currentDate.getMonth();
let currentYear = currentDate.getFullYear();


function createCalendar() {

    calendar.innerHTML = "";

    const months = [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ];

    monthName.textContent = `${months[currentMonth]} ${currentYear}`;

    const daysOfWeek = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];

    daysOfWeek.forEach(function(day) {
        const dayName = document.createElement("div");
        dayName.classList.add("day-name");
        dayName.textContent = day;
        calendar.appendChild(dayName);
    });

    const firstDay = new Date(currentYear, currentMonth, 1).getDay();
    const numberOfDays = new Date(currentYear, currentMonth + 1, 0).getDate();

    // Only show assignments that haven't been deleted
    const visibleAssignments = allAssignments.filter(function(assignment) {
        return !hiddenAssignments.includes(assignment.assignment_id);
    });

    // Empty spaces before the first day
    for (let i = 0; i < firstDay; i++) {
        const emptyDay = document.createElement("div");
        emptyDay.classList.add("day");
        emptyDay.classList.add("other-month");
        calendar.appendChild(emptyDay);
    }

    // All the days
    for (let day = 1; day <= numberOfDays; day++) {

        const dayElement = document.createElement("div");
        dayElement.classList.add("day");

        const dayNumber = document.createElement("div");

        dayElement.textContent = day;

        visibleAssignments.forEach(function(assignment) {

        const dueDate = new Date(
            assignment.due_date + "T00:00:00"
        );

        if (
            dueDate.getDate() === day &&
            dueDate.getMonth() === currentMonth &&
            dueDate.getFullYear() === currentYear
        ) {

            const assignmentElement = document.createElement("div");

            assignmentElement.classList.add("assignment");

            assignmentElement.textContent =
                assignment.course_name + ": " +
                assignment.assignment_name;

            dayElement.appendChild(assignmentElement);

        }

        });

        calendar.appendChild(dayElement);

    }

}


// -------------------------
// PREVIOUS / NEXT MONTH
// -------------------------

previousMonth.addEventListener("click", function() {
    currentMonth--;
    if (currentMonth < 0) {
        currentMonth = 11;
        currentYear--;
    }
    createCalendar();
});

nextMonth.addEventListener("click", function() {
    currentMonth++;
    if (currentMonth > 11) {
        currentMonth = 0;
        currentYear++;
    }
    createCalendar();
});


// Build everything when the page loads
createTodoList();
createCalendar();
syncToFile();