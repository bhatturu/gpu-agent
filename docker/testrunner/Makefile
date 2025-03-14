TESTRUNNER_IMAGE ?= $(DOCKER_REGISTRY)/$(TESTRUNNER_IMAGE_NAME):$(TESTRUNNER_IMAGE_TAG)
# name used for saving the container images as tar.gz
DOCKER_CONTAINER_IMAGE ?= $(TESTRUNNER_IMAGE_NAME)-$(TESTRUNNER_IMAGE_TAG)
HOURLY_TAG_LABEL ?= latest

.PHONY: docker
docker:
	@echo "building amd testrunner container"
	@echo "HOURLY_TAG : $(HOURLY_TAG_LABEL)"
	@cp $(TOP_DIR)/bin/amd-test-runner .
	@docker build --build-arg BASE_IMAGE=$(RHEL_BASE_IMAGE) --build-arg RHEL_REPO_URL=$(RHEL_REPO_URL) -t ${TESTRUNNER_IMAGE} . -f $(TOP_DIR)/docker/testrunner/Dockerfile
	@rm -rf $(TOP_DIR)/docker/testrunner/amd-test-runner

.PHONY: docker-cicd
docker-cicd:
	@echo "building amd testrunner container"
	@echo "HOURLY_TAG : $(HOURLY_TAG_LABEL)"
	@cp $(TOP_DIR)/bin/amd-test-runner .
	@docker build --build-arg BASE_IMAGE=$(RHEL_BASE_IMAGE) --build-arg RHEL_REPO_URL=$(RHEL_REPO_URL) -t ${TESTRUNNER_IMAGE} --label HOURLY_TAG=$(HOURLY_TAG_LABEL) . -f $(TOP_DIR)/docker/testrunner/Dockerfile
	@rm -rf $(TOP_DIR)/docker/testrunner/amd-test-runner

.PHONY: docker-save
docker-save: ## save the container
	@echo "saving docker image to $(DOCKER_CONTAINER_IMAGE).tar.gz"
	@docker save $(IMG) | gzip > $(DOCKER_CONTAINER_IMAGE).tar.gz
