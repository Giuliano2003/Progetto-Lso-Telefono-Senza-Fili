#!/bin/bash
set -e

host="libretranslate"
port=5000
url="http://$host:$port"

echo "Aspettando che LibreTranslate sia pronto su $url ..."

while true; do
  # Prova a fare una richiesta GET alla root e controlla che risponda
  if curl -s "$url" > /dev/null; then
    echo "LibreTranslate è pronto!"
    break
  else
    echo "LibreTranslate non ancora pronto, riprovo tra 10 secondi..."
    sleep 10
  fi
done

# Esegui il comando passato al container
exec "$@"
