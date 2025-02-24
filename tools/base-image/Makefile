all:
	@docker build --build-arg USER=$(shell id -un) --build-arg GROUP=$(shell id -gn) \
		--build-arg UID=$(shell id -u) --build-arg GID=$(shell id -g) -t ${BUILD_CONTAINER} .
	@echo "dev container build complete : ${BUILD_CONTAINER}"
