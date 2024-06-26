all: build
	docker-compose --env-file .testenv up -d

build:
	docker-compose --env-file .testenv build

down:
	docker-compose --env-file .testenv down

update:
	docker-compose --env-file .testenv pull
