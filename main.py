from fastapi import FastAPI
from pydantic import BaseModel
import sqlite3
from datetime import datetime
import json
import os
import uvicorn


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
        kcal_per_100 REAL NOT NULL,
        meal_type TEXT NOT NULL
    )
    """)

    c.execute("""
    CREATE TABLE IF NOT EXISTS logs (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        food TEXT NOT NULL,
        amount REAL NOT NULL,
        kcal REAL NOT NULL,
        date TEXT NOT NULL,
        time TEXT NOT NULL
    )
    """)

    foods = load_foods_from_file()

    for f in foods:
        c.execute(
            "INSERT OR IGNORE INTO foods (name, kcal_per_100, meal_type) VALUES (?, ?, ?)",
            (f["name"], f["kcal_per_100"], f["meal_type"])
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
    amount: float


@app.post("/log")
def log_food(data: FoodLog):
    conn = get_conn()
    c = conn.cursor()

    c.execute("SELECT kcal_per_100 FROM foods WHERE name = ?", (data.food,))
    row = c.fetchone()

    if not row:
        conn.close()
        return {"error": "unknown food"}

    kcal = (row[0] * data.amount) / 100.0
    now = datetime.now()
    date = now.strftime("%Y-%m-%d")
    time = now.strftime("%H:%M")

    c.execute(
        "INSERT INTO logs (food, amount, kcal, date, time) VALUES (?, ?, ?, ?, ?)",
        (data.food, data.amount, kcal, date, time),
    )

    conn.commit()
    conn.close()

    return {"food": data.food, "amount": data.amount, "kcal": kcal, "time": time}


@app.get("/today")
def today():
    conn = get_conn()
    c = conn.cursor()

    today_date = datetime.now().strftime("%Y-%m-%d")
    c.execute(
        "SELECT food, amount, kcal, time FROM logs WHERE date = ? ORDER BY id ASC",
        (today_date,),
    )
    rows = c.fetchall()
    conn.close()

    total = sum(r[2] for r in rows)

    return {
        "total_kcal": total,
        "items": [
            {"food": r[0], "amount": r[1], "kcal": r[2], "time": r[3]}
            for r in rows
        ],
    }


@app.get("/foods")
def foods():
    conn = get_conn()
    c = conn.cursor()

    meal = get_meal_type()

    c.execute("""
        SELECT name, kcal_per_100
        FROM foods
        WHERE meal_type = ? OR meal_type = 'any'
        ORDER BY name ASC
    """, (meal,))

    rows = c.fetchall()
    conn.close()

    return {
        "foods": [
            {"name": r[0], "kcal_per_100": r[1]}
            for r in rows
        ]
    }


if __name__ == "__main__":
    uvicorn.run(
        "main:app",
        host="0.0.0.0",
        port=8000,
        reload=True
    )
