<!-----
NEW: Check the "Suppress top comment" option to remove this info from the output.

Conversion time: 1.376 seconds.


Using this Markdown file:

1. Paste this output into your source file.
2. See the notes and action items below regarding this conversion run.
3. Check the rendered output (headings, lists, code blocks, tables) for proper
   formatting and use a linkchecker before you publish this page.

Conversion notes:

* Docs to Markdown version 1.0β29
* Tue Oct 27 2020 01:53:12 GMT-0700 (PDT)
* Source doc: DIGI MEMO SUPPORT Linux Driver for LX-IP
* Tables are currently converted to HTML tables.
* This document has images: check for >>>>>  gd2md-html alert:  inline image link in generated source and store images to your server. NOTE: Images in exported zip file from Google Docs may not appear in  the same order as they do in your doc. Please check the images!

----->

# Installing the Linux driver for LX-IP, LX-IP MADI, LX-MADI

* [Option 1 : Automatic installation](#option-1--automatic-installation)
  * [I) Download and install the driver](#i-download-and-install-the-driver)
    * [&gt; Debian, Ubuntu and other Debian-based distributions](#-debian-ubuntu-and-other-debian-based-distributions)
      * [I.1) Install the dependencies](#i1-install-the-dependencies)
      * [I.2) Download the driver package](#i2-download-the-driver-package)
      * [I.3) Install the driver package](#i3-install-the-driver-package)
    * [&gt; Fedora, CentOS, openSUSE or other RPM-based distributions](#-fedora-centos-opensuse-or-other-rpm-based-distributions)
      * [I.1) Install the dependencies](#i1-install-the-dependencies-1)
      * [I.2) Download the driver package](#i2-download-the-driver-package-1)
      * [I.3) Install the driver package](#i3-install-the-driver-package-1)
    * [&gt; Any Linux distribution with a DKMS-enabled package manager](#-any-linux-distribution-with-a-dkms-enabled-package-manager)
      * [I.1) Install the dependencies](#i1-install-the-dependencies-2)
      * [I.2) Download the driver sources](#i2-download-the-driver-sources)
      * [I.3) Install the driver](#i3-install-the-driver)
  * [II) Check the installation](#ii-check-the-installation)
* [Option 2 : Manually building and installing the driver](#option-2--manually-building-and-installing-the-driver)
  * [I) Get the source code](#i-get-the-source-code)
  * [II) Compile the driver](#ii-compile-the-driver)
  * [III) Install and load the driver](#iii-install-and-load-the-driver)

## Option 1 : Automatic installation

This method works on most major distributions, by using the DKMS system. After the initial installation, nothing more is needed to be done as DKMS will take care of re-building the driver every time the kernel is updated, or a new kernel is installed.

**<span style="text-decoration:underline;">	Requirements : </span>**

*   A package manager which supports DKMS. Most major distributions support this, please check your distribution’s documentation if you are not sure.
*   DKMS
*   The kernel headers for the kernel version(s) you need
*   Git, if you don’t use a Debian-based or RPM-based distribution

### I) Download and install the driver

#### > Debian, Ubuntu and other Debian-based distributions

##### I.1) Install the dependencies

```
$ sudo apt install dkms linux-headers-$(uname -r)
```

This will allow you to install the driver for the currently running kernel. If you have other kernels you wish to install it for, you will also need to install their headers by adding <code>linux-headers-<em>&lt;kernelversion></em></code> to the above command (replace <code><em>&lt;kernelversion></em></code> with the version you need)

##### I.2) Download the driver package

Go [here](https://github.com/Digigram-audio/lxipmadi) to download the package. To do that, check out to the latest version (`3.1.2` at the time of writing) :

# IMAGE

![alt_text](images/image1.png "image_tooltip")

Then, download the Debian package located in the `src/LX-dkms` directory :

# IMAGE

![alt_text](images/image2.png "image_tooltip")

##### I.3) Install the driver package

Go into the directory where you downloaded the package (we have downloaded it in `~/digigram` in our example), and install the package :

```
$ cd ~/digigram
$ sudo apt install ./lxipmadi-dkms_3.1.2_amd64.deb
```

#### > Fedora, CentOS, openSUSE or other RPM-based distributions

##### I.1) Install the dependencies

*   For Fedora/CentOS/Red Hat :

    ```
    $ sudo yum install dkms kernel-devel
    ```

*   For SUSE/openSUSE :

    ```
    $ sudo zypper install dkms kernel-devel
    ```

This will allow you to install the driver for the currently running kernel. If you have other kernels you wish to install it for, refer to your distribution’s documentation for installing the relevant kernel headers.

##### I.2) Download the driver package

Go [here](https://github.com/Digigram-audio/lxipmadi) to download the package. To do that, check out to the latest version (`3.1.2` at the time of writing) :

# IMAGE

![alt_text](images/image3.png "image_tooltip")

Then, download the RPM package located in the `LX-dkms` directory :

# IMAGE

![alt_text](images/image4.png "image_tooltip")

##### I.3) Install the driver package

Go into the directory where you downloaded the package (we have downloaded it in `~/digigram` in our example), and install the package :

*   For Fedora/CentOS/Red Hat :

    ```
    $ cd ~/digigram
    $ sudo yum install ./lxipmadi-3.1.2-1dkms.noarch.rpm
    ```

*   SUSE/openSUSE :

    ```
    $ cd ~/digigram
    $ sudo zypper install ./lxipmadi-3.1.2-1dkms.noarch.rpm
    ```

#### > Any Linux distribution with a DKMS-enabled package manager

##### I.1) Install the dependencies

Before the next steps, the following packages should be installed through	your distribution’s package manager :

*   <span style="text-decoration:underline;">Git</span> : the package is usually called simply `git`
*   <span style="text-decoration:underline;">DKMS</span> : the package is usually called simply `dkms`
*   <span style="text-decoration:underline;">Kernel headers for all kernel versions you need</span> :
    *   The package name is <code>linux-headers-<em>&lt;kernelversion></em></code> on Debian/ Ubuntu and their derivatives.
    *   The package name is <code>kernel-devel</code> on most RPM-based distributions such as Fedora, CentOS or openSUSE.
    *   On Arch Linux, it is called <code><em>&lt;linuxflavor></em>-headers</code>, depending on what kernel flavor you are using. For example : <code>linux-headers</code>, or <code>linux-lts-headers</code>, or <code>linux-zen-headers</code>, etc.
    *   For another distribution, please refer to its documentation.

##### I.2) Download the driver sources

Create a working directory and go into it. In our example, it will be `~/digigram/github_repo` :

```
$ mkdir -p ~/digigram/github_repo
$ cd ~/digigram/github_repo
```

Clone the [Digigram Github repository](https://github.com/Digigram-audio/lxipmadi) :

```
$ git clone https://github.com/Digigram-audio/lxipmadi
```

Go into the cloned repository, list the available version tags, and check out to the latest one (`3.1.2` at the time of writing) :

```
$ cd Alsa
$ git tag
[...]
3.1.2
$ git checkout 3.1.2
```

Copy the `LX` directory to <code>/usr/src/lxipmadi-<em>&lt;version></em></code>, replacing <code><em>&lt;version></em></code> with the one from the previous command (the target directory has to have exactly that name). In our example, the command is :

```
$ sudo cp -r ./LX /usr/src/lxipmadi-3.1.2
```

##### I.3) Install the driver

Use DKMS to install the driver for the current kernel :

```
$ sudo dkms install lxipmadi/3.1.2
```

### II) Check the installation

After completing one of the previous set of steps depending on your distribution, run the following command to check which kernels the driver has been installed for (example output on an Ubuntu machine with several kernels installed) :

```
$ sudo dkms status
lxipmadi, 3.1.2, 5.3.0-18-generic, x86_64: installed
lxipmadi, 3.1.2, 5.3.0-40-lowlatency, x86_64: installed
lxipmadi, 3.1.2, 5.3.0-46-generic, x86_64: installed
```

If you can see the relevant kernel versions with the `installed` tag, everything should work fine. If a kernel version you need is missing, you can run the following command to install it, provided that you have already installed the headers (see step 1 of the instructions for your distribution) :

```
$ sudo dkms install lxipmadi/3.1.2 -k <kernelversion>
```

From now on, the driver will be automatically loaded at boot, and for every kernel update, the package manager will automatically run DKMS to build it. If you want to test right away without a reboot, you can manually load the driver with :

```
$ sudo modprobe snd-lxip
```

for LX-IP and LX-IP MADI, or :

```
$ sudo modprobe snd-lxmadi
```

for LX-MADI.

## Option 2 : Manually building and installing the driver

This method works for any Linux system, but requires to be done every time the kernel is updated as it doesn’t use any package manager to automatically rebuild the driver.

**<span style="text-decoration:underline;">	Requirements : </span>**

*   The kernel headers for the kernel version(s) you need
*   Git
*   Make and a C compiler such as gcc

Please refer to your distribution’s documentation to install these requirements.

### I) Get the source code

Clone the [Digigram Github repository](https://github.com/Digigram-audio/lxipmadi) :

```
$ git clone https://github.com/Digigram-audio/lxipmadi
```

Go into the cloned repository, list the available version tags, and check out to the latest one (`3.1.2` at the time of writing) :

```
$ cd Alsa
$ git tag
[...]
3.1.2
$ git checkout 3.1.2
```

### II) Compile the driver

Go into the `LX` directory, and compile the driver by simply running `make` :

```
$ cd src/LX
$ make
```

### III) Install and load the driver

The compilation has generated two kernel modules : `snd-lxip.ko` and `snd-lxmadi.ko`. Copy these two modules to the kernel’s modules directory :

```
$ sudo mkdir -p /lib/modules/<kernelversion>/kernel/digigram
$ sudo cp snd-lx*.ko /lib/modules/<kernelversion>/kernel/digigram
```

Then run `depmod` to update the module dependencies :

```
$ sudo depmod -a
```

At this point, the driver should automatically be loaded at boot if the card is plugged in. To test right away without a reboot, load the following module depending on the LX model you use :

For LX-IP and LX-IP MADI :

```
$ sudo modprobe snd-lxip
```

For LX-MADI :

```
$ sudo modprobe snd-lxmadi
```
