/***************************************************************************
            fstab.c  -  Access the /etc/fstab file
                             -------------------
    begin                : Mon Dec 26 2004
    copyright            : (C) 2004 by Markus Raab
    email                : elektra@markus-raab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the BSD License (revised).                      *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This is a backend that takes /etc/fstab file as its backend storage.  *
 *   The kdbGet() method will parse /etc/fstab and generate a              *
 *   valid key tree. The kdbSet() method will take a KeySet with valid     *
 *   filesystem keys and print an equivalent regular fstab in stdout.      *
 *                                                                         *
 ***************************************************************************/

#include "fstab.h"

#define MAX_NUMBER_SIZE 10

ssize_t elektraFstabGet(KDB *handle, KeySet *returned, const Key *parentKey)
{
	int errnosave = errno;
	ssize_t nr_keys = 0;
	Key *key;
	Key *dir;
	FILE *fstab=0;
	struct mntent *fstabEntry;
	char fsname[MAX_PATH_LENGTH];
	char buffer[MAX_NUMBER_SIZE];

#if DEBUG && VERBOSE
	printf ("get fstab %s from %s", keyName(parentKey), keyString(parentKey));
#endif

	ksClear (returned);
	key = keyDup (parentKey);
	ksAppendKey(returned, key);
	nr_keys ++;

	fstab=setmntent(keyString(parentKey), "r");
	if (fstab == 0)
	{
		/* propagate errno */
		errno = errnosave;
		return -1;
	}
	
	while ((fstabEntry=getmntent(fstab)))
	{
		unsigned int swapIndex=0;
		nr_keys += 7;

		/* Some logic to define the filesystem name when it is not
		 * so obvious */
		if (!strcmp(fstabEntry->mnt_type,"swap")) {
			sprintf(fsname,"swap%02d",swapIndex);
			swapIndex++;
		} else if (!strcmp(fstabEntry->mnt_dir,"none")) {
			strcpy(fsname,fstabEntry->mnt_type);
		} else if (!strcmp(fstabEntry->mnt_dir,"/")) {
			strcpy(fsname,"rootfs");
		} else {
			/* fsname will be the mount point without '/' char */
			char *slash=0;
			char *curr=fstabEntry->mnt_dir;
			fsname[0]=0;
			
			while((slash=strchr(curr,PATH_SEPARATOR))) {
				if (slash==curr) {
					curr++;
					continue;
				}
				
				strncat(fsname,curr,slash-curr);
				curr=slash+1;
			}
			strcat(fsname,curr);
		}
		
		/* Include only the filesystem pseudo-names */
		dir = keyDup (parentKey);
		keyAddBaseName(dir, fsname);
		keySetString(dir,"");
		keySetComment(dir,"");
		keySetMode(dir, 0664); /* TODO stat */
		keySetComment (dir, "Filesystem pseudo-name");
		ksAppendKey(returned,dir);

		key = keyDup (dir);
		keyAddBaseName(key, "device");
		keySetString (key, fstabEntry->mnt_fsname);
		keySetComment (key, "Device or Label");
		ksAppendKey(returned, key);

		key = keyDup (dir);
		keyAddBaseName(key, "mpoint");
		keySetString (key, fstabEntry->mnt_dir);
		keySetComment (key, "Mount point");
		ksAppendKey(returned, key);

		key = keyDup (dir);
		keyAddBaseName(key, "type");
		keySetString (key, fstabEntry->mnt_type);
		keySetComment (key, "Filesystem type.");
		ksAppendKey(returned, key);

		key = keyDup (dir);
		keyAddBaseName(key, "options");
		keySetString (key, fstabEntry->mnt_opts);
		keySetComment (key, "Filesystem specific options");
		ksAppendKey(returned, key);

		key = keyDup (dir);
		keyAddBaseName(key, "dumpfreq");
		snprintf(buffer, MAX_NUMBER_SIZE, "%d",fstabEntry->mnt_freq);
		keySetString (key, buffer);
		keySetComment (key, "Dump frequency in days");
		ksAppendKey(returned, key);

		key = keyDup (dir);
		keyAddBaseName(key, "passno");
		snprintf(buffer, MAX_NUMBER_SIZE, "%d",fstabEntry->mnt_passno);
		keySetString (key, buffer);
		keySetComment (key, "Pass number on parallel fsck");
		ksAppendKey(returned, key);

		keySetDir (dir);
	}
	
	endmntent(fstab);

	errno = errnosave;
	return nr_keys;
}


ssize_t elektraFstabSet(KDB *handle, KeySet *ks, const Key *parentKey)
{
	int ret = 1;
	int errnosave = errno;
	FILE *fstab=0;
	Key *key=0;
	char *basename = 0;
	const void *rootname = 0;
	struct mntent fstabEntry;

#if DEBUG && VERBOSE
	printf ("set fstab %s from file %s\n", keyName(parentKey), keyString(parentKey));
#endif

	ksRewind (ks);
	if ((key = ksNext (ks)) != 0)
	{
		unlink(keyString(parentKey));
		return ret;
	} /*skip parent key*/

	fstab=setmntent(keyString(parentKey), "w");
	memset(&fstabEntry,0,sizeof(struct mntent));

	while ((key = ksNext (ks)) != 0)
	{
		ret ++;
		basename=strrchr(keyName(key), '/')+1;
#if DEBUG && VERBOSE
		printf ("key: %s %s\n", keyName(key), basename);
#endif
		if (!strcmp (basename, "device"))
		{
			fstabEntry.mnt_fsname=(char *)keyValue(key);
		} else if (!strcmp (basename, "mpoint")) {
			fstabEntry.mnt_dir=(char *)keyValue(key);
		} else if (!strcmp (basename, "type")) {
			fstabEntry.mnt_type=(char *)keyValue(key);
		} else if (!strcmp (basename, "options")) {
			fstabEntry.mnt_opts=(char *)keyValue(key);
		} else if (!strcmp (basename, "dumpfreq")) {
			fstabEntry.mnt_freq=atoi((char *)keyValue(key));
		} else if (!strcmp (basename, "passno")) {
			fstabEntry.mnt_passno=atoi((char *)keyValue(key));
		} else { // new rootname
			if (!rootname)
			{
				rootname = keyValue(key);
			} else {
				rootname = keyValue(key);
#if DEBUG && VERBOSE
				fprintf(stdout, "first: %s   %s   %s   %s   %d %d\n",
					fstabEntry.mnt_fsname,
					fstabEntry.mnt_dir,
					fstabEntry.mnt_type,
					fstabEntry.mnt_opts,
					fstabEntry.mnt_freq,
					fstabEntry.mnt_passno);
#endif
				addmntent(fstab, &fstabEntry);
				memset(&fstabEntry,0,sizeof(struct mntent));
			}
		}
	}

	if (rootname)
	{
#if DEBUG && VERBOSE
		fprintf(stdout, "last: %s   %s   %s   %s   %d %d\n",
			fstabEntry.mnt_fsname,
			fstabEntry.mnt_dir,
			fstabEntry.mnt_type,
			fstabEntry.mnt_opts,
			fstabEntry.mnt_freq,
			fstabEntry.mnt_passno);
#endif
		addmntent(fstab, &fstabEntry);
	}
	
	endmntent(fstab);
	errno = errnosave;
	return ret;
}


Plugin *ELEKTRA_PLUGIN_EXPORT(fstab) {
	return elektraPluginExport("fstab",
		ELEKTRA_PLUGIN_GET,            &elektraFstabGet,
		ELEKTRA_PLUGIN_SET,            &elektraFstabSet,
		ELEKTRA_PLUGIN_END);
}


