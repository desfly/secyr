FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1 PYTHONPATH=/app
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt \
    && groupadd --gid 10001 signer \
    && useradd --uid 10001 --gid signer --no-create-home signer \
    && mkdir /state && chown signer:signer /state
COPY signer.py server.py ./
USER 10001:10001
EXPOSE 9080
ENTRYPOINT ["python", "server.py"]
CMD ["--config", "/run/homeguard/signer.json", "--database", "/state/signer.sqlite", "--bind", "0.0.0.0"]
