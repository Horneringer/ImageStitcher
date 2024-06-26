FROM gitlab.npomis.ru:5050/8_departament/share/docker_qt_5.15:latest

RUN bash /APTaddUnstable.sh

RUN bash /APTaddStable.sh

RUN apt purge -y *apache*

RUN apt install -y gepcalcmodule gepclientmodule

COPY /project/ /project 

COPY /opt /opt

WORKDIR /project/

RUN qmake && make && make install

COPY docker/docker-entrypoint.sh /docker-entrypoint.sh

RUN apt install -y acl && groupadd -f -g 3988 gepdb && usermod -aG 3988 www-data

RUN chmod +x /docker-entrypoint.sh

ENTRYPOINT ["/docker-entrypoint.sh"]
