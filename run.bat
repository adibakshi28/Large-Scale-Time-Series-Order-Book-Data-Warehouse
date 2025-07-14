@echo off
REM ----------------------------------------
REM run.bat — bootstrap & run the Flask UI
REM ----------------------------------------

REM 1) Go into the UI folder
cd UI

REM 2) Create the venv if it doesn't exist
IF NOT EXIST venv (
    echo Creating Python virtual environment...
    python -m venv venv
)

REM 3) Activate the venv
echo Activating virtual environment...
call venv\Scripts\activate

REM 4) Install requirements
echo Installing requirements...
pip install --upgrade pip
pip install -r ..\requirements.txt

REM 5) Launch the Flask app
echo Starting Flask UI...
python app.py

REM 6) Pause so you can read any errors
pause
