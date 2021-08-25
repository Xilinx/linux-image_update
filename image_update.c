// SPDX-License-Identifier: MIT
/*
 * Xilinx Zynq MPSoC Firmware layer
 *
 *  Copyright (C) 2021 Xilinx, Inc.
 *
 *  Vikram Sreenivasa Batchali <bvikram@xilinx.com>
 */

#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Error Codes */
#define XST_SUCCESS			(0x0)
#define XST_FAILURE			(0x1)

/* Macros */
#define SYS_CHECKSUM_OFFSET		(0x3U)
#define XBIU_IDEN_STR_OFFSET		(0x24U)
#define XBIU_IDEN_STR_LEN		(0x4U)
#define XBIU_QSPI_MFG_INFO_SIZE		(0x100U)
#define XBIU_IMG_REVISON_OFFSET		(0x70U)
#define XBIU_IMG_REVISON_SIZE		(0x24U)

/* The below enums denote persistent registers in Qspi Flash */
struct sys_persistent_state {
	char last_booted_img;
	char requested_boot_img;
	char img_b_bootable;
	char img_a_bootable;
};

struct sys_boot_img_info {
	char idstr[4U];
	unsigned int ver;
	unsigned int len;
	unsigned int checksum;
	struct sys_persistent_state persistent_state;
	unsigned int boot_img_a_offset;
	unsigned int boot_img_b_offset;
	unsigned int recovery_img_offset;
} __packed;

enum sys_boot_img_id {
	SYS_BOOT_IMG_A_ID = 0,
	SYS_BOOT_IMG_B_ID = 1,
};

/* Function Declarations */
static unsigned int calculate_checksum(void);
static int update_image(char *qspi_mtd_file);
static int read_image_file(char *input_file);
static int validate_board_string(void);
static int update_persistent_registers(char *qspi_mtd_pers_reg_file);
static void calculate_image_checksum(char *srcaddr, unsigned int len,
				     unsigned int *calc_crc);
static int verify_current_running_image(char *qspi_mtd_file);
static int validate_boot_img_info(void);
static int print_persistent_state(char *qspi_mtd_file);
static int print_qspi_mfg_info(void);
static void print_usage(void);
static int print_image_rev_info(char *qspi_mtd_file, char *image_name);

/* Variable definitions */
static char *srcaddr = NULL;
static unsigned int image_size;
static struct sys_boot_img_info boot_img_info __attribute__ ((aligned(4U)));

static const unsigned int crc_table[] = {
	0x00000000U, 0x77073096U, 0xEE0E612CU, 0x990951BAU,
	0x076DC419U, 0x706AF48FU, 0xE963A535U, 0x9E6495A3U,
	0x0EDB8832U, 0x79DCB8A4U, 0xE0D5E91EU, 0x97D2D988U,
	0x09B64C2BU, 0x7EB17CBDU, 0xE7B82D07U, 0x90BF1D91U,
	0x1DB71064U, 0x6AB020F2U, 0xF3B97148U, 0x84BE41DEU,
	0x1ADAD47DU, 0x6DDDE4EBU, 0xF4D4B551U, 0x83D385C7U,
	0x136C9856U, 0x646BA8C0U, 0xFD62F97AU, 0x8A65C9ECU,
	0x14015C4FU, 0x63066CD9U, 0xFA0F3D63U, 0x8D080DF5U,
	0x3B6E20C8U, 0x4C69105EU, 0xD56041E4U, 0xA2677172U,
	0x3C03E4D1U, 0x4B04D447U, 0xD20D85FDU, 0xA50AB56BU,
	0x35B5A8FAU, 0x42B2986CU, 0xDBBBC9D6U, 0xACBCF940U,
	0x32D86CE3U, 0x45DF5C75U, 0xDCD60DCFU, 0xABD13D59U,
	0x26D930ACU, 0x51DE003AU, 0xC8D75180U, 0xBFD06116U,
	0x21B4F4B5U, 0x56B3C423U, 0xCFBA9599U, 0xB8BDA50FU,
	0x2802B89EU, 0x5F058808U, 0xC60CD9B2U, 0xB10BE924U,
	0x2F6F7C87U, 0x58684C11U, 0xC1611DABU, 0xB6662D3DU,
	0x76DC4190U, 0x01DB7106U, 0x98D220BCU, 0xEFD5102AU,
	0x71B18589U, 0x06B6B51FU, 0x9FBFE4A5U, 0xE8B8D433U,
	0x7807C9A2U, 0x0F00F934U, 0x9609A88EU, 0xE10E9818U,
	0x7F6A0DBBU, 0x086D3D2DU, 0x91646C97U, 0xE6635C01U,
	0x6B6B51F4U, 0x1C6C6162U, 0x856530D8U, 0xF262004EU,
	0x6C0695EDU, 0x1B01A57BU, 0x8208F4C1U, 0xF50FC457U,
	0x65B0D9C6U, 0x12B7E950U, 0x8BBEB8EAU, 0xFCB9887CU,
	0x62DD1DDFU, 0x15DA2D49U, 0x8CD37CF3U, 0xFBD44C65U,
	0x4DB26158U, 0x3AB551CEU, 0xA3BC0074U, 0xD4BB30E2U,
	0x4ADFA541U, 0x3DD895D7U, 0xA4D1C46DU, 0xD3D6F4FBU,
	0x4369E96AU, 0x346ED9FCU, 0xAD678846U, 0xDA60B8D0U,
	0x44042D73U, 0x33031DE5U, 0xAA0A4C5FU, 0xDD0D7CC9U,
	0x5005713CU, 0x270241AAU, 0xBE0B1010U, 0xC90C2086U,
	0x5768B525U, 0x206F85B3U, 0xB966D409U, 0xCE61E49FU,
	0x5EDEF90EU, 0x29D9C998U, 0xB0D09822U, 0xC7D7A8B4U,
	0x59B33D17U, 0x2EB40D81U, 0xB7BD5C3BU, 0xC0BA6CADU,
	0xEDB88320U, 0x9ABFB3B6U, 0x03B6E20CU, 0x74B1D29AU,
	0xEAD54739U, 0x9DD277AFU, 0x04DB2615U, 0x73DC1683U,
	0xE3630B12U, 0x94643B84U, 0x0D6D6A3EU, 0x7A6A5AA8U,
	0xE40ECF0BU, 0x9309FF9DU, 0x0A00AE27U, 0x7D079EB1U,
	0xF00F9344U, 0x8708A3D2U, 0x1E01F268U, 0x6906C2FEU,
	0xF762575DU, 0x806567CBU, 0x196C3671U, 0x6E6B06E7U,
	0xFED41B76U, 0x89D32BE0U, 0x10DA7A5AU, 0x67DD4ACCU,
	0xF9B9DF6FU, 0x8EBEEFF9U, 0x17B7BE43U, 0x60B08ED5U,
	0xD6D6A3E8U, 0xA1D1937EU, 0x38D8C2C4U, 0x4FDFF252U,
	0xD1BB67F1U, 0xA6BC5767U, 0x3FB506DDU, 0x48B2364BU,
	0xD80D2BDAU, 0xAF0A1B4CU, 0x36034AF6U, 0x41047A60U,
	0xDF60EFC3U, 0xA867DF55U, 0x316E8EEFU, 0x4669BE79U,
	0xCB61B38CU, 0xBC66831AU, 0x256FD2A0U, 0x5268E236U,
	0xCC0C7795U, 0xBB0B4703U, 0x220216B9U, 0x5505262FU,
	0xC5BA3BBEU, 0xB2BD0B28U, 0x2BB45A92U, 0x5CB36A04U,
	0xC2D7FFA7U, 0xB5D0CF31U, 0x2CD99E8BU, 0x5BDEAE1DU,
	0x9B64C2B0U, 0xEC63F226U, 0x756AA39CU, 0x026D930AU,
	0x9C0906A9U, 0xEB0E363FU, 0x72076785U, 0x05005713U,
	0x95BF4A82U, 0xE2B87A14U, 0x7BB12BAEU, 0x0CB61B38U,
	0x92D28E9BU, 0xE5D5BE0DU, 0x7CDCEFB7U, 0x0BDBDF21U,
	0x86D3D2D4U, 0xF1D4E242U, 0x68DDB3F8U, 0x1FDA836EU,
	0x81BE16CDU, 0xF6B9265BU, 0x6FB077E1U, 0x18B74777U,
	0x88085AE6U, 0xFF0F6A70U, 0x66063BCAU, 0x11010B5CU,
	0x8F659EFFU, 0xF862AE69U, 0x616BFFD3U, 0x166CCF45U,
	0xA00AE278U, 0xD70DD2EEU, 0x4E048354U, 0x3903B3C2U,
	0xA7672661U, 0xD06016F7U, 0x4969474DU, 0x3E6E77DBU,
	0xAED16A4AU, 0xD9D65ADCU, 0x40DF0B66U, 0x37D83BF0U,
	0xA9BCAE53U, 0xDEBB9EC5U, 0x47B2CF7FU, 0x30B5FFE9U,
	0xBDBDF21CU, 0xCABAC28AU, 0x53B39330U, 0x24B4A3A6U,
	0xBAD03605U, 0xCDD70693U, 0x54DE5729U, 0x23D967BFU,
	0xB3667A2EU, 0xC4614AB8U, 0x5D681B02U, 0x2A6F2B94U,
	0xB40BBE37U, 0xC30C8EA1U, 0x5A05DF1BU, 0x2D02EF8DU,
};

/* Function definitions */

/*****************************************************************************/
/**
 * @brief
 * This function is the main function. It takes the input file as parameter
 * and calls APIs to read it, validate it and write to Qspi, and read back
 * from Qspi for checksum validation. Upon successful validation, main
 * function calls APIs to mark the newly written image as requested image
 * to ensure the newly updated image boots.
 *
 * @param	argc is the number of arguments to main
 * @param	argv is expected to point to the name of the app followed by
 *		input image file
 *
 * @return	XST_SUCCESS on success and error code on failure
 *
 *****************************************************************************/
int main(int argc, char *argv[])
{
	int ret = XST_FAILURE;
	char qspi_mtd_file[20U] = {0U};
	char image_file_name[100U] = {0U};
	int opt;
	int update_flag = 0;
	int verify_flag = 0;
	int help_flag = 0;
	int print_flag = 0;

	while((opt = getopt(argc, argv, "hpvi")) != -1) {
		switch(opt)
		{
			case 'h':
			{
				help_flag = 1;
			}
				break;
				
			case 'p':
			{
				print_flag = 1;
			}
				break;
			case 'v':
			{
				verify_flag = 1;
			}
				break;
			case 'i':
			{
				update_flag = 1;
				if (strlen(argv[optind]) <
					sizeof(image_file_name)) {
					strcpy(image_file_name, argv[optind]);
				}
			}
				break;
			default:
			{
				printf("Invalid option!\n");
				print_usage();
				return ret;
			}
		}
	}

	if (help_flag == 1) {
		print_usage();
		return XST_SUCCESS;
	}
	if ((print_flag | verify_flag | update_flag) == 0) {
		printf("Invalid command format!\n");
		print_usage();
		return ret;
	}

	if (print_flag == 1) {
		ret = print_persistent_state("/dev/mtd2");
		if (ret != XST_SUCCESS) {
			printf("Reading persistent registers ");
			printf("backup\n");
			ret = print_persistent_state("/dev/mtd3");
		}
		if (ret == XST_SUCCESS) {
			ret = print_qspi_mfg_info();
		}
		if (ret != XST_SUCCESS) {
			return ret;
		}
	}

	if ((verify_flag == 0) && (update_flag == 0)) {
		/* image_update has been called with -p option only
		 * and the command has been processed.
		 */
		return XST_SUCCESS;
	}


	/* Validate board string to ensure the app does not run on
	 * unsupported boards
	 */
	ret = validate_board_string();
	if (ret != XST_SUCCESS)
		return XST_FAILURE;

	ret = verify_current_running_image("/dev/mtd2");
	if (ret != XST_SUCCESS) {
		printf("Reading persistent registers backup\n");
		ret = verify_current_running_image("/dev/mtd3");
		if (ret != XST_SUCCESS) {
			printf("Unable to retrieve persistent registers\n");
			return XST_FAILURE;
		}
	}

	printf("Marking last booted image as bootable\n");
	ret = update_persistent_registers("/dev/mtd2");
	if (ret < 0)
		return XST_FAILURE;

	ret = update_persistent_registers("/dev/mtd3");
	if (ret < 0)
		return XST_FAILURE;

	if (update_flag == 0) {
		return ret;
	}

	printf("Reading Image..\n");
	ret = read_image_file(image_file_name);
	if (ret != XST_SUCCESS)
		goto END;

	/* Input image would be written to a Qspi partition that does not
	 * contain the current running image
	 */
	if (boot_img_info.persistent_state.last_booted_img ==
		(char)SYS_BOOT_IMG_A_ID) {
		boot_img_info.persistent_state.img_b_bootable = 0U;
		strcpy(qspi_mtd_file, "/dev/mtd7");
	} else {
		boot_img_info.persistent_state.img_a_bootable = 0U;
		strcpy(qspi_mtd_file, "/dev/mtd5");
	}

	printf("Marking target image non bootable\n");
	ret = update_persistent_registers("/dev/mtd2");
	if (ret < 0)
		goto END;

	ret = update_persistent_registers("/dev/mtd3");
	if (ret < 0)
		goto END;

	printf("Writing Image..\n");
	ret = update_image(qspi_mtd_file);
	if (ret != XST_SUCCESS)
		goto END;

	printf("Marking target image as non bootable and requested image\n");
	if (boot_img_info.persistent_state.last_booted_img ==
		(char)SYS_BOOT_IMG_A_ID) {
		boot_img_info.persistent_state.requested_boot_img =
			(char)SYS_BOOT_IMG_B_ID;
	} else {
		boot_img_info.persistent_state.requested_boot_img =
			(char)SYS_BOOT_IMG_A_ID;
	}
	/* Update persistent register partition */
	ret = update_persistent_registers("/dev/mtd2");
	if (ret < 0)
		goto END;

	/* Update persistent register backup partition */
	ret = update_persistent_registers("/dev/mtd3");
	if (ret < 0)
		goto END;

	printf("%s updated successfully\n", image_file_name);

END:
	if (srcaddr)
		free(srcaddr);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function calculates the checksum of boot_img_info which reflects the
 * persistent registers in Qspi.
 *
 * @return	Checksum of boot_img_info variable
 *
 *****************************************************************************/
static unsigned int calculate_checksum(void)
{
	unsigned int idx;
	unsigned int checksum = 0U;
	unsigned int *data = (unsigned int *)&boot_img_info;
	unsigned int boot_img_info_size = sizeof(boot_img_info) / 4U;

	for (idx = 0U; idx < SYS_CHECKSUM_OFFSET; idx++)
		checksum += data[idx];

	for (idx = SYS_CHECKSUM_OFFSET + 1U; idx < boot_img_info_size; idx++)
		checksum += data[idx];

	return (0xFFFFFFFFU - checksum);
}

/*****************************************************************************/
/**
 * @brief
 * This function writes boot_img_info variable to persistent registers
 * indicated by qspi_mtd_file.
 *
 * @param	qspi_mtd_file denotes the mtd partition to be updated
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int update_persistent_registers(char *qspi_mtd_pers_reg_file)
{
	int fd_pers_reg, ret = XST_FAILURE;
	erase_info_t ei = {0U};
	mtd_info_t qspi_mtd_info;

	fd_pers_reg = open(qspi_mtd_pers_reg_file, O_WRONLY);
	if (fd_pers_reg < 0) {
		printf("Open Qspi MTD partition failed\n");
		return ret;
	}

	ret = ioctl(fd_pers_reg, MEMGETINFO, &qspi_mtd_info);
	if (ret != XST_SUCCESS) {
		printf("retrieving MTD paartition info failed\n");
		goto END;
	}

	/* Update persistent registers in Qspi */
	ei.start = 0;
	ei.length = qspi_mtd_info.size;
	ret = ioctl(fd_pers_reg, MEMERASE, &ei);
	if (ret < 0) {
		printf("Erase Qspi MTD partition failed\n");
		goto END;
	}

	ret = lseek(fd_pers_reg, 0, SEEK_SET);
	if (ret != 0) {
		printf("Seek Qspi MTD partition failed\n");
		goto END;
	}

	boot_img_info.checksum = calculate_checksum();
	ret = write(fd_pers_reg, (char *)&boot_img_info, sizeof(boot_img_info));
	if (ret != sizeof(boot_img_info)) {
		printf("Write Qspi MTD partition failed\n");
		ret = XST_FAILURE;
		goto END;
	}
	ret = XST_SUCCESS;

END:
	close(fd_pers_reg);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function reads the persistent registers indicated by qspi_mtd_file
 * to marked the recently updated image as bootable and target image as
 * non bootable.
 *
 * @param	qspi_mtd_file denotes the mtd partition to be updated
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int verify_current_running_image(char *qspi_mtd_file)
{
	int fd_pers_reg, ret = XST_FAILURE;

	fd_pers_reg = open(qspi_mtd_file, O_RDONLY);
	if (fd_pers_reg < 0) {
		printf("Open Qspi MTD partition failed\n");
		return ret;
	}

	ret = read(fd_pers_reg, (char *)&boot_img_info, sizeof(boot_img_info));
	if (ret != sizeof(boot_img_info)) {
		printf("Read Qspi MTD partition failed\n");
		ret = XST_FAILURE;
		goto END;
	}

	ret = validate_boot_img_info();
	if (ret != XST_SUCCESS) {
		printf("Persistent registers are corrupted\n");
		goto END;
	}

	if (boot_img_info.persistent_state.last_booted_img ==
		(char)SYS_BOOT_IMG_A_ID) {
		if (boot_img_info.persistent_state.img_a_bootable == 0U)
			boot_img_info.persistent_state.img_a_bootable = 1U;
	} else {
		if (boot_img_info.persistent_state.img_b_bootable == 0U)
			boot_img_info.persistent_state.img_b_bootable = 1U;
	}

END:
	close(fd_pers_reg);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function copies the contents of input image file to local memory.
 * It validates the image by checking for "XLNX" identification string.
 *
 * @param	input_file is the input image file
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int read_image_file(char *input_file)
{
	int fp, ret = XST_FAILURE;
	struct stat image_details;
	const char  *iden_str = "XNLX";
	char *iden_str_ptr = NULL;

	/* Open Image file and read contents */
	fp = open(input_file, O_RDONLY);
	if (fp < 0) {
		printf("Input image file open failed\n");
		return ret;
	}

	ret = fstat(fp, &image_details);
	if (ret != XST_SUCCESS) {
		printf("Input image file stat read failed\n");
		goto END;
	}

	image_size = image_details.st_size;
	srcaddr = (char *)calloc(image_size, 1);
	if (!srcaddr) {
		printf("Allocation of memory for input image failed\n");
		ret = XST_FAILURE;
		goto END;
	}
	ret = read(fp, srcaddr, image_size);
	if (ret != image_size) {
		printf("Input image file read failed\n");
		ret = XST_FAILURE;
		goto END;
	}

	/* Validate Identification String of Image */
	iden_str_ptr = &srcaddr[XBIU_IDEN_STR_OFFSET];
	if (strncmp(iden_str_ptr, iden_str, XBIU_IDEN_STR_LEN) != 0) {
		printf("Identification String Validation of image Failed!!\n");
		ret = XST_FAILURE;
		goto END;
	}
	ret = XST_SUCCESS;

END:
	close(fp);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function checks if the input image fits in Qspi partition. If yes, it
 * erases Qspi partition and writes the image to Qspi. The function then
 * compares checksums of input image file and data written in Qspi to validates
 * image write operation.
 *
 * @param	qspi_mtd_file denotes the mtd partition to be updated
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int update_image(char *qspi_mtd_file)
{
	int fd, ret = XST_FAILURE;
	erase_info_t ei = {0U};
	mtd_info_t qspi_mtd_info;
	unsigned int input_image_checksum = 0xFFFFFFFFU;
	unsigned int qspi_image_checksum = 0xFFFFFFFFU;
	char read_buffer[1024U];
	unsigned int idx, len;

	/* Qspi operations */
	fd = open(qspi_mtd_file, O_RDWR);
	if (fd < 0) {
		printf("Open Qspi MTD partition failed\n");
		return ret;
	}

	ret = ioctl(fd, MEMGETINFO, &qspi_mtd_info);
	if (ret != XST_SUCCESS) {
		printf("retrieving MTD paartition info failed\n");
		goto END;
	}

	/* Validate Image Size */
	if (image_size > qspi_mtd_info.size) {
		printf("Image file too big to update. Update aborted\n");
		ret = XST_FAILURE;
		goto END;
	}

	ei.start = 0;
	ei.length = qspi_mtd_info.size;
	ret = ioctl(fd, MEMERASE, &ei);
	if (ret < 0) {
		printf("Erase Qspi MTD partition failed\n");
		goto END;
	}

	ret = lseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		printf("Seek Qspi MTD partition failed\n");
		goto END;
	}

	ret = write(fd, (char *)srcaddr, image_size);
	if (ret != image_size) {
		printf("Write to Qspi MTD partition failed\n");
		ret = XST_FAILURE;
		goto END;
	}

	ret = lseek(fd, 0, SEEK_SET);
	if (ret != 0) {
		printf("Seek Qspi MTD partition failed\n");
		goto END;
	}

	/* Calculate and Validate checksum */
	calculate_image_checksum(srcaddr, image_size, &input_image_checksum);
	while (image_size > 0U) {
		if (image_size > 1024U)
			len = 1024U;
		else
			len = image_size;

		ret = read(fd, read_buffer, len);
		if (ret != len) {
			printf("Qspi checksum calculation failed\n");
			ret = XST_FAILURE;
			goto END;
		}
		calculate_image_checksum(read_buffer, len,
					 &qspi_image_checksum);
		image_size -= len;
	}
	if (input_image_checksum != qspi_image_checksum) {
		printf("checksum mismatch!! Image update failed.\n");
		goto END;
	}
	ret = XST_SUCCESS;

END:
	close(fd);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function reads the Board revision from eeprom and checks if
 * the application is running on one of the supported boards.
 *
 * @return	XST_SUCCESS if board revision is supported, else XST_FAILURE
 *
 *****************************************************************************/
static int validate_board_string(void)
{
	int ret = XST_FAILURE;
	FILE *cmd;
	char revision[10U] = {0U};

	cmd = popen("fru-print.py -b som -f revision", "r");
	if (!cmd) {
		printf("Unable to read Board revision from EEprom\n");
		return ret;
	}
	fscanf(cmd, "%9s", revision);
	if ((strcmp(revision, "A") == 0) || (strcmp(revision, "B") == 0) ||
	    (strcmp(revision, "Y") == 0) || (strcmp(revision, "Z") == 0) ||
		(strcmp(revision, "1") == 0)) {
		ret = XST_SUCCESS;
	} else {
		printf("Unable to read Board revision from EEprom via ");
		printf("fru-print.py utility\n");
	}
	pclose(cmd);

	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function calculates checksum of len bytes of data starting from srcaddr
 * and stores it in calc_crc.
 *
 * @param	srcaddr points to the start of data
 * @param	len denotes number of bytes of data
 * @param	calc_crc is a place holder for the calculated checksum
 *
 * @return	None
 *
 *****************************************************************************/
static void calculate_image_checksum(char *srcaddr, unsigned int len,
				     unsigned int *calc_crc)
{
	unsigned int idx;

	for (idx = 0U; idx < len; idx++) {
		*calc_crc = ((*calc_crc) >> 8U) ^
			crc_table[((*calc_crc) ^ srcaddr[idx]) & 0xFFU];
	}
}

/*****************************************************************************/
/**
 * @brief
 * This function checks for identification string and validates checksum of
 * boot_img_info, which at the point of calling this function is populated with
 * values of persistent registers.
 *
 * @return	XST_SUCCESS on success and error code on failure
 *
 *****************************************************************************/
static int validate_boot_img_info(void)
{
	int ret = XST_FAILURE;
	unsigned int checksum = boot_img_info.checksum;

	if ((boot_img_info.idstr[0U] == 'A') &&
	    (boot_img_info.idstr[1U] == 'B') &&
		(boot_img_info.idstr[2U] == 'U') &&
		(boot_img_info.idstr[3U] == 'M')) {
		boot_img_info.checksum = calculate_checksum();
		if (checksum == boot_img_info.checksum)
			ret = XST_SUCCESS;
	}

	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function reads the persistent registers and displays the state of
 * images A and B in a readable format.
 *
 * @param	qspi_mtd_file denotes the mtd partition to be read
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int print_persistent_state(char *qspi_mtd_file)
{
	int fd_pers_reg, ret = XST_FAILURE;

	fd_pers_reg = open(qspi_mtd_file, O_RDONLY);
	if (fd_pers_reg < 0) {
		printf("Open Qspi MTD partition failed\n");
		return ret;
	}

	ret = read(fd_pers_reg, (char *)&boot_img_info, sizeof(boot_img_info));
	if (ret != sizeof(boot_img_info)) {
		printf("Read Qspi MTD partition failed\n");
		ret = XST_FAILURE;
		goto END;
	}

	ret = validate_boot_img_info();
	if (ret != XST_SUCCESS) {
		printf("Persistent registers are corrupted\n");
		goto END;
	}

	printf("Image A: ");
	if (boot_img_info.persistent_state.img_a_bootable == 0U)
		printf("Non Bootable\n");
	else
		printf("Bootable\n");

	printf("Image B: ");
	if (boot_img_info.persistent_state.img_b_bootable == 0U)
		printf("Non Bootable\n");
	else
		printf("Bootable\n");

	printf("Requested Boot Image: ");
	if (boot_img_info.persistent_state.requested_boot_img ==
		(char)SYS_BOOT_IMG_A_ID)
		printf("Image A\n");
	else
		printf("Image B\n");

	printf("Last Booted Image: ");
	if (boot_img_info.persistent_state.last_booted_img ==
		(char)SYS_BOOT_IMG_A_ID)
		printf("Image A\n");
	else
		printf("Image B\n");

END:
	close(fd_pers_reg);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function reads qspi mtd partition and prints Qspi MFG info.
 *
 * @param	qspi_mtd_file denotes the mtd partition to be read
 * @param	image_name is the string denoting ImageA or ImageB
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int print_image_rev_info(char *qspi_mtd_file, char *image_name)
{
	int fd, ret = XST_FAILURE;
	char image_rev_info[XBIU_IMG_REVISON_SIZE + 1U] = {0};

	fd = open(qspi_mtd_file, O_RDONLY);
	if (fd < 0) {
		printf("Open Qspi MTD partition failed\n");
		return ret;
	}

	ret = lseek(fd, XBIU_IMG_REVISON_OFFSET, SEEK_SET);
	if (ret != XBIU_IMG_REVISON_OFFSET) {
		printf("Seek Qspi MTD partition failed\n");
		goto END;
	}

	ret = read(fd, image_rev_info, XBIU_IMG_REVISON_SIZE);
	if (ret != XBIU_IMG_REVISON_SIZE) {
		printf("Read Qspi MTD partition failed\n");
		ret = XST_FAILURE;
		goto END;
	}

	if (image_rev_info[0U] == 0) {
		strncpy(image_rev_info, "Not defined", XBIU_IMG_REVISON_SIZE);
	}
	printf("%s Revision Info: %s\n", image_name, image_rev_info);
	ret = XST_SUCCESS;

END:
	close(fd);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function reads qspi mtd partition and prints Qspi MFG info.
 *
 * @return	XST_SUCCESS on SUCCESS and error code on failure
 *
 *****************************************************************************/
static int print_qspi_mfg_info(void)
{
	int fd_pers_reg, ret = XST_FAILURE;
	char qspi_mfg_info[XBIU_QSPI_MFG_INFO_SIZE + 1U] = {0};

	fd_pers_reg = open("/dev/mtd14", O_RDONLY);
	if (fd_pers_reg < 0) {
		printf("Open Qspi MTD partition failed\n");
		return ret;
	}

	ret = read(fd_pers_reg, qspi_mfg_info, XBIU_QSPI_MFG_INFO_SIZE);
	if (ret != XBIU_QSPI_MFG_INFO_SIZE) {
		printf("Read Qspi MTD partition failed\n");
		ret = XST_FAILURE;
		goto END;
	}

	printf("%s\n", qspi_mfg_info);

	ret = print_image_rev_info("/dev/mtd5", "ImageA");
	if (ret == XST_SUCCESS) {
		ret = print_image_rev_info("/dev/mtd7", "ImageB");
	}

END:
	close(fd_pers_reg);
	return ret;
}

/*****************************************************************************/
/**
 * @brief
 * This function prints information regarding usage of image_update utility.
 *
 * @return	None
 *
 *****************************************************************************/
static void print_usage(void)
{
	printf("Usage: sudo image_update -i <path of image file>\n");
	printf("image_update -i updates qspi image with the image file ");
	printf("passed as argument.\n");
	printf("image_update -p prints persistent state registers.\n");
	printf("image_update -v marks the current running image as ");
	printf("bootable.\n");
	printf("image_update -h prints this menu.\n");
	printf("Can use xmutil bootfw_update instead of image_update in ");
	printf("any of the above commands.\n");
}

