#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include <linux/fs.h>
#include "testimg.h"

#define MAX_PATH_LEN 255

struct ext4_super_block superblock;
struct ext4_group_desc group_desc;
struct ext4_inode root_dir, sub_dir, dest_file;
struct ext4_dir_entry_2 ext4_dir;
char *pt = NULL, *res_pt = NULL; //pt is the current pointer, res_pt for some result
char *orig_pt = NULL, *orig_res = NULL;
FILE* fp = NULL;

__le32 block_lv1[1024 / 4];

int flag = 0;
unsigned int inode_tb_off = 0;
unsigned int block_tb_off = 0;
unsigned int inodes_per_group = 0;
unsigned int inode_size = 0x80, root_dir_off = 0;

unsigned dest_inode_off = 0;


int process_path_header()
{
	switch(*pt){
	case '.':
		++pt;
		if((*pt) == '.'){
			++pt;
		}else{
		}
	case '/':
		++pt;
		get_name();
		return 0; //0 represent the root dir
	}
	return -1; //else, something wrong
}

int get_name()
{
	//printf("\nDBG in get_name():\n");
	orig_pt = pt;
	orig_res = res_pt;
	//printf("%d\n\n", *pt);
	if('\0' == (*pt))
		return 1; // 1 means error
	while((*pt) != '\0' && (*pt) != '/'){
		(*res_pt) = (*pt);
		++res_pt;
		++pt;
	}
	*(res_pt) = '\0';
	//printf("\nDBG orig_res is %s\n", orig_res);
	if((*pt) == '/')
		++pt; //escape '/' or '\0'
	return 0; // 0 means ok
}

void check_file_type(int type)
{
	switch(type)
	{
	case 0x0:printf("Unknown\n"); break;
	case 0x1:printf("Regular file\n"); break;
	case 0x2:printf("Directory\n"); break;
	case 0x3:printf("Character device file\n"); break;
	case 0x4:printf("Block device file"); break;
	case 0x5:printf("FIFO\n"); break;
	case 0x6:printf("Socket\n"); break;
	case 0x7:printf("Symbolic link\n"); break;
	default: break;
	}
}

void dir()
{
	struct ext4_dir_entry_2 tmp_dir = {};
        int name_len = 0, cnt = 0, find_flag = 0;
        memset(&ext4_dir, 0, sizeof(struct ext4_dir_entry_2));
        while(1)
        { // read all subdirs
                name_len = 0;
                cnt = 0;
                fread(&ext4_dir, 8, 1, fp); // the length of name will change

                if(ext4_dir.inode == 0)
                        break;
                char tmp = '\0';
                do
                {
                        fread(&tmp, 1, 1, fp);
                        ext4_dir.name[cnt] = tmp;
                        ++cnt;
                }while(tmp != '\0' && cnt < EXT4_NAME_LEN);
                ext4_dir.name[cnt] = '\0';
                name_len = cnt - 1;

                fseek(fp, (name_len / 4 + 1) * 4 - name_len - 1, SEEK_CUR);
                if(strcmp(orig_res, ext4_dir.name) == 0){
			memcpy(&tmp_dir, &ext4_dir, sizeof(struct ext4_dir_entry_2));
			find_flag = 1;
			flag = 1;
                }
		printf("Subdir name : %s\n", ext4_dir.name);
                printf("Inode index number: %u, offset: 0x%x\n", ext4_dir.inode, (ext4_dir.inode - 1)*inode_size + inode_tb_off*0x400);
                printf("Type: %u   ", ext4_dir.file_type);
		check_file_type(ext4_dir.file_type);
        }
	if(1 == find_flag)
	{
		printf("\n\n=====================================\nFound %s!!!!!!!!!!!!!!!!!!!!!!!!!\n", orig_res);
                dest_inode_off = (tmp_dir.inode - 1)*inode_size + inode_tb_off*0x400;
                printf("Its inode offset is 0x%x\n", dest_inode_off);
	}
}

void get_blocks_lv1(int iblock)
{
	int i = 0;
	printf("\n===========================================\n");
	printf("Level1 indirect blocks:(1024): ");

	fseek(fp, iblock*0x400, SEEK_SET);
	fread(block_lv1, 1024, 1, fp);
	for(i = 0; i < 256; ++i)
	{
		if(block_lv1[i] == 0 || block_lv1[i] == 0x54545454)
			continue;
		printf("Block %d: 0x%x\n", i + 12, block_lv1[i]);
	}
}

void get_blocks_lv2(int iblock)
{
	
}

void get_extent(struct ext4_extent *eExtent)
{
	int start_block = (eExtent->ee_start_hi << 32) + eExtent->ee_start_lo;
	printf("Start block is %u, consume %u blocks\n", start_block, eExtent->ee_len);
}

void get_extent_idx(struct ext4_extent_idx *eExtent, unsigned int ext4_off)
{
	struct ext4_extent_header eheader = {};
	struct ext4_extent_idx eidx = {};
        struct ext4_extent extent = {};

	unsigned int offset = (eExtent->ei_leaf_hi << 32)+eExtent->ei_leaf_lo;
	fseek(fp, offset*0x400, SEEK_SET);
	fread(&eheader, sizeof(struct ext4_extent_header), 1, fp);
	
	int num = eheader.eh_entries;
	if(eheader.eh_depth >0){
		int i = 0;
		for(i = 0; i < num; ++i){
                        fseek(fp, ext4_off + sizeof(struct ext4_extent_header) + sizeof(struct ext4_extent_idx)*i, SEEK_SET);
                        fread(&eidx, sizeof(struct ext4_extent_idx), 1, fp);
                        get_extent_idx(&eidx, ext4_off);
                }
	}else{
		int i = 0;
		for(i = 0; i < num; ++i){
                        fseek(fp, ext4_off + sizeof(struct ext4_extent_header) + sizeof(struct ext4_extent)*i, SEEK_SET);
                        fread(&extent, sizeof(struct ext4_extent), 1, fp);
                        get_extent(&extent);
                }
	}
}

void get_extent_tree(struct ext4_inode *dest_file)
{
	struct ext4_extent_header eheader = {};
	struct ext4_extent_idx eidx = {};
	struct ext4_extent extent = {};

	fseek(fp, dest_file->i_block[4]*0x400, SEEK_SET);
	fread(&eheader, sizeof(struct ext4_extent_header), 1, fp);
	int num = eheader.eh_entries;
	//printf("DBGDBG!!! %d %d\n", eheader.eh_depth, eheader.eh_entries);

	if(eheader.eh_depth > 0){
		int i = 0;
		for(i = 0; i < num; ++i){
			fseek(fp, dest_file->i_block[4]*0x400 + sizeof(struct ext4_extent_header) + sizeof(struct ext4_extent_idx)*i, SEEK_SET);
			fread(&eidx, sizeof(struct ext4_extent_idx), 1, fp);
			get_extent_idx(&eidx, dest_file->i_block[4]*0x400);
		}
	}else{
		int i = 0;
		for(i = 0; i < num; ++i){
			fseek(fp, dest_file->i_block[4]*0x400 + sizeof(struct ext4_extent_header) + sizeof(struct ext4_extent)*i, SEEK_SET);
                        fread(&extent, sizeof(struct ext4_extent), 1, fp);
			get_extent(&extent);
		}
	}
}

void get_all_blocks(struct ext4_inode dest_file)
{
	//get direct blocks
	printf("\n\n=======================================\n");
	
	if(dest_file.i_flags == 0x80000){
		printf("This file uses extent tree.\n\n");
		get_extent_tree(&dest_file);
	}else{
        	printf("All the blocks are(0-11,direct;  12-14, indirect ):\n");

        	int i = 0;
        	for(i = 0; i < 15; ++i)
        	{
                	printf("Block %d: offset - 0x%x\n", i, dest_file.i_block[i]*0x400);
        	}
	
		if(dest_file.i_block[12] != 0)
			get_blocks_lv1(dest_file.i_block[12]);
		//if(dest_file.i_block[13] != 0)
		//	get_blocks_lv2(dest_file.i_block[13]);
		//if(i_block[14] != 0)
        	//        get_blocks_lv3(i_block[14]);
	}
}

int main()
{

	char path[MAX_PATH_LEN], res_tmp[MAX_PATH_LEN];
	printf("Please input the path: ");
	scanf("%255s", path);
	pt = path;
	res_pt = res_tmp;
	process_path_header();
	//printf("The path %s\n", res_tmp);

	fp = fopen("./onehundred.img", "rb");
	if(NULL == fp){
		printf("open file error\n");
		exit(1);
	}

	//printf("First ingore the first block(empty)\n");
	fseek(fp, 1024, SEEK_SET);

	printf("Reading super block.\n");
	fread(&superblock, 1024, 1, fp);

	inodes_per_group = superblock.s_inodes_per_group;
	printf("In all there are %u groups\n", superblock.s_inodes_count / inodes_per_group);
	fseek(fp, 1024*2, SEEK_SET);
	fread(&group_desc, 1024, 1, fp);
	inode_tb_off = group_desc.bg_inode_table_lo; //offset of inode table
	printf("The offset of inode table is 0x%x\n", inode_tb_off*0x400);
	
	root_dir_off = (2 - 1)*inode_size + inode_tb_off*0x400;
	printf("\n============================================\nThe offset of root dir is 0x%x\n", root_dir_off);
	
	fseek(fp, root_dir_off, SEEK_SET);
	fread(&root_dir, 0x80, 1, fp);
	fseek(fp, root_dir.i_block[5]*0x400, SEEK_SET);
	flag = 0;
	dir();
	printf("The offset of the content is 0x%x\n", root_dir.i_block[5]*0x400);

	if(!flag)
	{
		printf("\n\nNo such file or directory '%s'\n", orig_res);
		return 0;
	}
	//printf("%s is found!!!!!!!!!!!!!!!!!!!!!!!!!1\n", orig_res);
	if(*pt == 0){
		printf("DONE!!!\n");
		return 0;
	}
	
	fseek(fp, dest_inode_off, SEEK_SET);
        fread(&sub_dir, 0x80, 1, fp);
	printf("subdir_content offset is 0x%x\n", sub_dir.i_block[5]*0x400);
        fseek(fp, sub_dir.i_block[5]*0x400, SEEK_SET);
	flag = 0;
	get_name();
	dir();
	
	if(!flag)
        {
                printf("\n\nNo such file or directory '%s'\n", orig_res);
                return 0;
        }
        //printf("%s is found, search deeper or choose to end\n", orig_res);

	fseek(fp, dest_inode_off, SEEK_SET);
        fread(&dest_file, 0x80, 1, fp);
	//printf("DBG !!!  %u\n", dest_inode_off);
	//printf("DBG!!!!  0x%x\n", dest_file.i_flags);
	get_all_blocks(dest_file);

	return 0;

}


