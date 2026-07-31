FROM node:24.18.0-bookworm-slim

WORKDIR /tools
COPY tools/markdown/package.json tools/markdown/package-lock.json ./
RUN npm ci --ignore-scripts \
    && npm cache clean --force

ENV PATH="/tools/node_modules/.bin:${PATH}"
WORKDIR /workspace

CMD ["markdownlint-cli2", "README.md", "FRITZMONITOR.md", "QUICK-SETUP.md", "COPYRIGHT.md"]
