#!/bin/sh

set -eu

echo "About to run the tests.  First we'll launch a gnome-session DBus mock."

python3 -m dbusmock --session org.gnome.SessionManager /org/gnome/SessionManager org.gnome.SessionManager &
sleep 1

gdbus call --session \
      --dest org.gnome.SessionManager \
      --object-path /org/gnome/SessionManager \
      --method org.freedesktop.DBus.Mock.AddTemplate 'tests/dbusmock/mock-gnome-session.py' '{}'

gdbus call --session \
      --dest org.gnome.SessionManager \
      --object-path /org/gnome/SessionManager \
      --method org.freedesktop.DBus.Mock.SetSessionRunning true

mkdir -p _build/tests/registryd

cd tests/registryd

pytest -v --junit-xml=../../_build/tests/registryd/registryd-pytest.junit.xml
