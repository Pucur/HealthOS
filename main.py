from fastapi import FastAPI
from pydantic import BaseModel
import sqlite3
from datetime import datetime
import json
import os
import uvicorn
from fastapi.responses import HTMLResponse

app = FastAPI()
DB = "healthos.db"


def get_conn():
    conn = sqlite3.connect(DB)
    return conn

def load_foods_from_file():
    if not os.path.exists("foods.json"):
        return []

    with open("foods.json", "r", encoding="utf-8") as f:
        return json.load(f)

def init_db():
    conn = get_conn()
    c = conn.cursor()

    c.execute("""
    CREATE TABLE IF NOT EXISTS foods (
        name TEXT PRIMARY KEY,
        kcal REAL NOT NULL,
        meal_type TEXT NOT NULL
    )
    """)

    c.execute("""
    CREATE TABLE IF NOT EXISTS logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        food TEXT NOT NULL,
        kcal REAL NOT NULL,
        date TEXT NOT NULL,
        time TEXT NOT NULL
    )
    """)

    foods = load_foods_from_file()

    for f in foods:
        c.execute(
            "INSERT OR IGNORE INTO foods (name, kcal, meal_type) VALUES (?, ?, ?)",
            (f["name"], f["kcal"], f["meal_type"])
        )

    conn.commit()
    conn.close()

init_db()

def get_meal_type():
    hour = datetime.now().hour

    if 5 <= hour < 11:
        return "breakfast"
    elif 11 <= hour < 16:
        return "lunch"
    else:
        return "dinner"

class FoodLog(BaseModel):
    food: str

@app.post("/log")
def log_food(data: FoodLog):
    conn = get_conn()
    c = conn.cursor()

    c.execute("SELECT kcal FROM foods WHERE name = ?", (data.food,))
    row = c.fetchone()

    if not row:
        conn.close()
        return {"error": "unknown food"}

    kcal = row[0]
    now = datetime.now()
    date = now.strftime("%Y-%m-%d")
    time = now.strftime("%H:%M")

    c.execute(
        "INSERT INTO logs (food, kcal, date, time) VALUES (?, ?, ?, ?)",
        (data.food, kcal, date, time),
    )

    conn.commit()
    conn.close()

    return {"food": data.food, "kcal": kcal, "time": time}


@app.get("/today")
def today():
    conn = get_conn()
    c = conn.cursor()

    today_date = datetime.now().strftime("%Y-%m-%d")
    c.execute(
        "SELECT food, kcal, time FROM logs WHERE date = ? ORDER BY id ASC",
        (today_date,),
    )
    rows = c.fetchall()
    conn.close()

    total = sum(r[1] for r in rows)

    return {
        "total_kcal": total,
        "items": [
            {"food": r[0], "kcal": r[1], "time": r[2]}
            for r in rows
        ],
    }


@app.get("/foods")
def foods():
    conn = get_conn()
    c = conn.cursor()

    meal = get_meal_type()

    c.execute("""
        SELECT name, kcal
        FROM foods
        WHERE meal_type = ? OR meal_type = 'any'
        ORDER BY name ASC
    """, (meal,))

    rows = c.fetchall()
    conn.close()

    return {
        "foods": [
            {"name": r[0], "kcal": r[1]}
            for r in rows
        ]
    }

@app.get("/", response_class=HTMLResponse)
def ui():
    return """
<!DOCTYPE html>
<html>
<head>
    <title>HealthOS</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial;
            background: #1E2A3A;
            color: white;
            margin: 0;
            padding: 10px;
        }

        #kcal {
            text-align: center;
            margin: 10px;
            font-size: 20px;
            transition: color 0.4s ease;
        }

        .grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
            margin-top: 15px;
        }

        button {
            width: 30%;
            margin: 1.5%;
            padding: 15px;
            font-size: 14px;
            border: none;
            border-radius: 10px;
            background: #ff4d4d;
            color: white;
        }

        button:active {
            transform: scale(0.95);
        }
    </style>
</head>

<body>

<div id="kcal">Loading...</div>
<div class="grid" id="foodGrid"></div>

<script>

var API = window.location.origin;

function loadFoods() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", API + "/foods", true);

    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4 && xhr.status === 200) {
            var data = JSON.parse(xhr.responseText);

            var grid = document.getElementById("foodGrid");
            grid.innerHTML = "";

            for (var i = 0; i < data.foods.length; i++) {
                var food = data.foods[i];

                var btn = document.createElement("button");
                btn.innerHTML = food.name + "<br>" + food.kcal + " kcal";

                btn.onclick = (function(name) {
                    return function () {
                        var xhr2 = new XMLHttpRequest();
                        xhr2.open("POST", API + "/log", true);
                        xhr2.setRequestHeader("Content-Type", "application/json");
                        xhr2.send(JSON.stringify({food: name}));

                        loadToday();
                    };
                })(food.name);

                grid.appendChild(btn);
            }
        }
    };

    xhr.send();
}

function loadToday() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", API + "/today", true);

    xhr.onreadystatechange = function () {
        if (xhr.readyState === 4 && xhr.status === 200) {
            var data = JSON.parse(xhr.responseText);

            var kcalEl = document.getElementById("kcal");
            var total = data.total_kcal;

            kcalEl.innerHTML =
                "HealthOS || Mai kalória: " + total + " kcal";

            if (total <= 2000) {
                kcalEl.style.color = "#00C853"; // zöld
            } else if (total <= 2900) {
                kcalEl.style.color = "#FFD600"; // sárga
            } else {
                kcalEl.style.color = "#D50000"; // piros
            }

        }
    };

    xhr.send();
}

loadFoods();
loadToday();
setInterval(loadToday, 20000);

</script>

</body>
</html>
"""

if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=True
    )
