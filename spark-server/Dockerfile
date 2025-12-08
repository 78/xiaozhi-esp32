# Spark AI Server for Seeed AI Watcher
# Multi-stage build for smaller image

FROM python:3.11-slim as builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    libopus-dev \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Create virtual environment
RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Install Python dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Production stage
FROM python:3.11-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libopus0 \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Copy virtual environment from builder
COPY --from=builder /opt/venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Create app user
RUN useradd -m -u 1000 spark
USER spark

# Create data directory
RUN mkdir -p /home/spark/data/memories

# Set working directory
WORKDIR /app

# Copy application code
COPY --chown=spark:spark server/ .

# Environment variables
ENV PYTHONUNBUFFERED=1
ENV PYTHONDONTWRITEBYTECODE=1
ENV SERVER_HOST=0.0.0.0
ENV SERVER_PORT=8765
ENV VISION_PORT=8766
ENV MEMORY_DIR=/home/spark/data/memories

# Expose ports
EXPOSE 8765 8766

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8766/health')" || exit 1

# Run the server (starts both WebSocket and Vision servers)
CMD ["python", "start.py"]
