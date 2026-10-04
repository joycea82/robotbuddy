const addTaskButton = document.getElementById("addTaskButton");
const todoList = document.getElementById("todoList");

const calendar = document.getElementById("calendar");
const monthName = document.getElementById("monthName");
const previousMonth = document.getElementById("previousMonth");
const nextMonth = document.getElementById("nextMonth");


// -------------------------
// TO-DO LIST
// -------------------------

addTaskButton.addEventListener("click", function() {

    const task = prompt("What do you need to do?");

    if (task === null || task.trim() === "") {
        return;
    }

    const todo = document.createElement("div");
    todo.classList.add("todo");

    todo.innerHTML = `
        <input type="checkbox">
        <span>${task}</span>
    `;

    todoList.appendChild(todo);

});


// -------------------------
// CALENDAR
// -------------------------

let currentDate = new Date();

let currentMonth = currentDate.getMonth();
let currentYear = currentDate.getFullYear();


function createCalendar() {

    // Remove the old calendar
    calendar.innerHTML = "";

    // Month names
    const months = [
        "January",
        "February",
        "March",
        "April",
        "May",
        "June",
        "July",
        "August",
        "September",
        "October",
        "November",
        "December"
    ];

    // Display the month and year
    monthName.textContent = `${months[currentMonth]} ${currentYear}`;


    // Add the days of the week
    const daysOfWeek = [
        "Sun",
        "Mon",
        "Tue",
        "Wed",
        "Thu",
        "Fri",
        "Sat"
    ];

    daysOfWeek.forEach(function(day) {

        const dayName = document.createElement("div");

        dayName.classList.add("day-name");

        dayName.textContent = day;

        calendar.appendChild(dayName);

    });


    // Find what day of the week the month starts on
    const firstDay = new Date(currentYear, currentMonth, 1).getDay();

    // Find how many days are in this month
    const numberOfDays = new Date(
        currentYear,
        currentMonth + 1,
        0
    ).getDate();


    // Add empty spaces before the first day
    for (let i = 0; i < firstDay; i++) {

        const emptyDay = document.createElement("div");

        emptyDay.classList.add("day");
        emptyDay.classList.add("other-month");

        calendar.appendChild(emptyDay);

    }


    // Add all the days
    for (let day = 1; day <= numberOfDays; day++) {

        const dayElement = document.createElement("div");

        dayElement.classList.add("day");

        dayElement.textContent = day;

        calendar.appendChild(dayElement);

    }

}


// -------------------------
// PREVIOUS MONTH
// -------------------------

previousMonth.addEventListener("click", function() {

    currentMonth--;

    if (currentMonth < 0) {

        currentMonth = 11;
        currentYear--;

    }

    createCalendar();

});


// -------------------------
// NEXT MONTH
// -------------------------

nextMonth.addEventListener("click", function() {

    currentMonth++;

    if (currentMonth > 11) {

        currentMonth = 0;
        currentYear++;

    }

    createCalendar();

});


// Create calendar when page loads
createCalendar();