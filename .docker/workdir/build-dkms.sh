#!/bin/sh

THIS_SCRIPT=$(basename $0)

_ERROR_MESSAGE()
{
	LINE_NUMBER="$1"
	MESSAGE="$2"

	echo "::error file=${THIS_SCRIPT},line=${LINE_NUMBER}::${MESSAGE}"
}


# Build the DKMS packages for Debian/Ubuntu and Fedora/CentOS
for i in /lib/modules/* ; do
	{ 
		dkms build ${CARD_MODEL}/${DRIVER_VERSION} -k $(basename $i) && \
		dkms mkrpm ${CARD_MODEL}/${DRIVER_VERSION} -k $(basename $i) && \
		dkms mkdeb ${CARD_MODEL}/${DRIVER_VERSION} -k $(basename $i)
	} || _ERROR_MESSAGE ${LINENO} "Unable to build DKMS package"
done

RPM_PACKAGE="/var/lib/dkms/${CARD_MODEL}/${DRIVER_VERSION}/rpm/${CARD_MODEL}-${DRIVER_VERSION}-1dkms.noarch.rpm"
DEB_PACKAGE="/var/lib/dkms/${CARD_MODEL}/${DRIVER_VERSION}/deb/${CARD_MODEL}-dkms_${DRIVER_VERSION}_amd64.deb"

{
	mkdir -p "${OUTPUT_DIR}" && \
	cp "${RPM_PACKAGE}" "${OUTPUT_DIR}" && \
	cp "${DEB_PACKAGE}" "${OUTPUT_DIR}"
} || _ERROR_MESSAGE ${LINENO} "Unable to copy DKMS packages to ${OUTPUT_DIR}"

# Generate checksum file

CHECKSUM_ALGORITHM="sha512"
CHECKSUM_COMMAND="${CHECKSUM_ALGORITHM}sum"
CHECKSUM_FILE="${OUTPUT_DIR}/${CARD_MODEL}-${DRIVER_VERSION}.${CHECKSUM_ALGORITHM}"

( cd ${OUTPUT_DIR} && ${CHECKSUM_COMMAND} -b * > ${CHECKSUM_FILE} )

echo "::set-output name=rpm_package::$(basename ${RPM_PACKAGE})"
echo "::set-output name=deb_package::$(basename ${DEB_PACKAGE})"
echo "::set-output name=checksum_file::$(basename ${CHECKSUM_FILE})"
