#!/bin/bash

cp -r queue /usr/
cp qserver_daemon /etc/init.d/

sed -i "s|YOUR_QUEUE_PATH|usr/queue|g" /etc/init.d/qserver_daemon

update-rc.d qserver_daemon defaults

/etc/init.d/qserver_daemon stop
/etc/init.d/qserver_daemon start

touch /etc/profile.d/queue.sh
echo "export PATH=$PATH:/usr/queue" >> /etc/profile.d/queue.sh


echo "Installation successful. Please logout and login again to use the tool"
