#include <kvm/util.h>
#include <kvm/kvm-cmd.h>
#include <kvm/builtin-setup.h>
#include <kvm/kvm.h>
#include <kvm/parse-options.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define KVM_PID_FILE_PATH	"/.kvm-tools/"
#define HOME_DIR		getenv("HOME")

static const char *instance_name;

static const char * const setup_usage[] = {
	"kvm setup [-n name]",
	NULL
};

static const struct option setup_options[] = {
	OPT_GROUP("General options:"),
	OPT_STRING('n', "name", &instance_name, "name", "Instance name"),
	OPT_END()
};

static void parse_setup_options(int argc, const char **argv)
{
	while (argc != 0) {
		argc = parse_options(argc, argv, setup_options, setup_usage,
				PARSE_OPT_STOP_AT_NON_OPTION);
		if (argc != 0)
			kvm_setup_help();
	}
}

void kvm_setup_help(void)
{
	usage_with_options(setup_usage, setup_options);
}

static int copy_file(const char *from, const char *to)
{
	int in_fd, out_fd;
	void *src, *dst;
	struct stat st;
	int err = -1;

	in_fd = open(from, O_RDONLY);
	if (in_fd < 0)
		return err;

	if (fstat(in_fd, &st) < 0)
		goto error_close_in;

	out_fd = open(to, O_RDWR | O_CREAT | O_TRUNC, st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO));
	if (out_fd < 0)
		goto error_close_in;

	src = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, in_fd, 0);
	if (src == MAP_FAILED)
		goto error_close_out;

	if (ftruncate(out_fd, st.st_size) < 0)
		goto error_munmap_src;

	dst = mmap(NULL, st.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, out_fd, 0);
	if (dst == MAP_FAILED)
		goto error_munmap_src;

	memcpy(dst, src, st.st_size);

	if (fsync(out_fd) < 0)
		goto error_munmap_dst;

	err = 0;

error_munmap_dst:
	munmap(dst, st.st_size);
error_munmap_src:
	munmap(src, st.st_size);
error_close_out:
	close(out_fd);
error_close_in:
	close(in_fd);

	return err;
}

static const char *guestfs_dirs[] = {
	"/dev",
	"/etc",
	"/home",
	"/host",
	"/proc",
	"/root",
	"/sys",
	"/tmp",
	"/var",
	"/var/lib",
	"/virt",
};

static const char *guestfs_symlinks[] = {
	"/bin",
	"/lib",
	"/lib64",
	"/sbin",
	"/usr",
};

static int copy_init(const char *guestfs_name)
{
	char path[PATH_MAX];

	snprintf(path, PATH_MAX, "%s%s%s/virt/init", HOME_DIR, KVM_PID_FILE_PATH, guestfs_name);

	return copy_file("guest/init", path);
}

static int make_guestfs_symlink(const char *guestfs_name, const char *path)
{
	char target[PATH_MAX];
	char name[PATH_MAX];

	snprintf(name, PATH_MAX, "%s%s%s%s", HOME_DIR, KVM_PID_FILE_PATH, guestfs_name, path);

	snprintf(target, PATH_MAX, "/host%s", path);

	return symlink(target, name);
}

static void make_root_dir(void)
{
	char name[PATH_MAX];

	snprintf(name, PATH_MAX, "%s%s", HOME_DIR, KVM_PID_FILE_PATH);

	mkdir(name, 0777);
}

static int make_dir(const char *dir)
{
	char name[PATH_MAX];

	snprintf(name, PATH_MAX, "%s%s%s", HOME_DIR, KVM_PID_FILE_PATH, dir);

	return mkdir(name, 0777);
}

static void make_guestfs_dir(const char *guestfs_name, const char *dir)
{
	char name[PATH_MAX];

	snprintf(name, PATH_MAX, "%s%s", guestfs_name, dir);

	make_dir(name);
}

void kvm_setup_resolv(const char *guestfs_name)
{
	char path[PATH_MAX];

	snprintf(path, PATH_MAX, "%s%s%s/etc/resolv.conf", HOME_DIR, KVM_PID_FILE_PATH, guestfs_name);

	copy_file("/etc/resolv.conf", path);
}

static int do_setup(const char *guestfs_name)
{
	unsigned int i;
	int ret;

	make_root_dir();

	ret = make_dir(guestfs_name);
	if (ret < 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(guestfs_dirs); i++)
		make_guestfs_dir(guestfs_name, guestfs_dirs[i]);

	for (i = 0; i < ARRAY_SIZE(guestfs_symlinks); i++) {
		make_guestfs_symlink(guestfs_name, guestfs_symlinks[i]);
	}

	return copy_init(guestfs_name);
}

int kvm_setup_create_new(const char *guestfs_name)
{
	return do_setup(guestfs_name);
}

int kvm_cmd_setup(int argc, const char **argv, const char *prefix)
{
	parse_setup_options(argc, argv);

	if (instance_name == NULL)
		kvm_setup_help();

	return do_setup(instance_name);
}