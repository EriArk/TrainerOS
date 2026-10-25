ARG PYTHON_IMAGE=python:3.12-slim-trixie
FROM ${PYTHON_IMAGE} AS runtime
ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /workspace/services/backend
COPY runtime-lock.txt ./
RUN pip install --no-cache-dir -r runtime-lock.txt
COPY pyproject.toml ./
COPY trainer_backend ./trainer_backend
RUN pip install --no-cache-dir --no-deps --no-build-isolation .
USER 10001:10001
CMD ["traineros-backend", "api"]

FROM runtime AS test
USER root
RUN apt-get update && apt-get install -y --no-install-recommends postgresql-17 && rm -rf /var/lib/apt/lists/*
COPY test-lock.txt ./
RUN pip install --no-cache-dir -r test-lock.txt
COPY tests ./tests
COPY contracts ./contracts
RUN useradd --uid 10001 --no-create-home backend-test && mkdir -p /workspace/build && chown 10001:10001 /workspace/build
ENV TRAINEROS_TEST_POSTGRES_BIN=/usr/lib/postgresql/17/bin
USER 10001:10001
CMD ["python", "-m", "pytest", "tests", "-q", "-p", "no:cacheprovider"]
