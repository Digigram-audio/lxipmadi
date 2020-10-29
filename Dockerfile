FROM centos:8

# Arguments from the `# docker run --build-args` command
ARG CARD_MODEL
ARG DRIVER_VERSION

# Transform arguments into persistent environment variables
ENV CARD_MODEL=${CARD_MODEL:-"undefined_soundcard"}
ENV DRIVER_VERSION=${DRIVER_VERSION:-"1.0-undef"}

# Built image will be moved here. This should be a host mount to get the output.
ENV OUTPUT_DIR="/output"

# Prepare the system with required packages
RUN 	dnf install -y epel-release && \
	dnf install -y 'dnf-command(config-manager)' && \
	dnf config-manager --set-enabled PowerTools && \
	dnf install -y \
		kernel-core \
		kernel-devel \
		dkms \
		rpm-build \
		fakeroot \
		debhelper

# Add the source code and a required DKMS config file
ADD ./src /usr/src/${CARD_MODEL}-${DRIVER_VERSION}
ADD ./.docker/etc/dkms/template-dkms-mkdeb /etc/dkms/template-dkms-mkdeb

# Working directory setup
ADD ./.docker/workdir /workdir

# Building script
ENTRYPOINT ["/workdir/build-dkms.sh"]
