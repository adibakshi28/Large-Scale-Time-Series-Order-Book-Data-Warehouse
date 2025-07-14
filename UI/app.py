import logging
import subprocess
import time
from flask import Flask, render_template, request, jsonify

app = Flask(__name__)

# Set up logging: timestamp + level + message
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(message)s",
    datefmt="%H:%M:%S"
)

BINARY = "../orderbook"

@app.route("/")
def index():
    return render_template(
        "index.html",
        symbols="SCH",
        start_ts="1609724964077464154",
        end_ts="1609724964129550454",
        fields="symbol,epoch,lastTradeQuantity,lastTradePrice,bid1p,bid1q,ask1p,ask1q,bid2p,bid2q,ask2p,ask2q"
    )

@app.route("/run", methods=["POST"])
def run_command():
    cmd = request.form.get("action")
    if cmd not in {"compile", "run", "query", "test", "clean"}:
        app.logger.warning("⚠️  Unknown action: %s", cmd)
        return "Unknown action", 400

    def run_and_log(args, **kwargs):
        cmd_str = " ".join(args)
        app.logger.info("🚀 Running> %s", cmd_str)
        start = time.monotonic()
        proc = subprocess.run(args, **kwargs)
        elapsed = time.monotonic() - start

        if proc.returncode == 0:
            app.logger.info("✅ Finished in %.3f s", elapsed)
        else:
            app.logger.error("❌ Failed (code %d) in %.3f s", proc.returncode, elapsed)

        return proc

    # Dispatch actions
    if cmd == "compile":
        run_and_log(["make", "clean"], cwd="..")
        proc = run_and_log(["make"], cwd="..", capture_output=True, text=True)

    elif cmd == "test":
        proc = run_and_log(["make", "test"], cwd="..", capture_output=True, text=True)

    elif cmd == "clean":
        proc = run_and_log(["make", "clean"], cwd="..", capture_output=True, text=True)

    elif cmd == "run":
        proc = run_and_log([BINARY], cwd="..", capture_output=True, text=True)

    else:  # query
        syms   = request.form["symbols"]
        ts1    = request.form["start_ts"]
        ts2    = request.form["end_ts"]
        fields = request.form["fields"].strip()
        query_cmd = [BINARY, "query", syms, ts1, ts2]
        if fields:
            query_cmd.append(fields)
        proc = run_and_log(query_cmd, cwd="..", capture_output=True, text=True)

    # Combine stdout/stderr for return
    output = proc.stdout or ""
    if proc.stderr:
        output += "\n\n" + proc.stderr
    return jsonify({"output": output})

if __name__ == "__main__":
    app.run(debug=True)
