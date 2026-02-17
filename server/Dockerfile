FROM python:3.11-slim

WORKDIR /app

# deps (inside /server context)
COPY requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r /app/requirements.txt

# backend code
COPY . /app

# dashboard folder is OUTSIDE build context, so we cannot COPY it from here
# We'll mount it in docker-compose instead (next step)

CMD ["python", "app.py"]

