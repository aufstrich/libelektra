/***************************************************************************
            dump.c  -  Skeleton of backends to access the Key Database
                             -------------------
    begin                : Mon May  3 15:22:44 CEST 2010
    copyright            : by Markus Raab
    email                : elektra@markus-raab.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the BSD License (revised).                      *
 *                                                                         *
 ***************************************************************************/

#include "dump.hpp"

using namespace ckdb;

#include <kdberrors.h>

namespace dump
{

int serialize(std::ostream &os, ckdb::Key *, ckdb::KeySet *ks)
{
	ckdb::Key *cur;

	os << "kdbOpen 1" << std::endl;

	os << "ksNew " << ckdb::ksGetSize(ks) << std::endl;

	ckdb::KeySet *metacopies = ckdb::ksNew(0);

	ksRewind(ks);
	while ((cur = ksNext(ks)) != 0)
	{
		size_t namesize = ckdb::keyGetNameSize(cur);
		size_t valuesize = ckdb::keyGetValueSize(cur);
		os << "keyNew " << namesize
		   << " " << valuesize << std::endl;
		os.write(ckdb::keyName(cur), namesize);
		os.write(static_cast<const char*>(ckdb::keyValue(cur)), valuesize);
		os << std::endl;

		const ckdb::Key *meta;
		ckdb::keyRewindMeta(cur);
		while ((meta = ckdb::keyNextMeta(cur)) != 0)
		{
			std::stringstream ss;
			ss << "user/" << meta; // use the address of pointer as name

			ckdb::Key *search = ckdb::keyNew(
					ss.str().c_str(),
					KEY_END);
			ckdb::Key *ret = ksLookup(metacopies, search, 0);

			if (!ret)
			{
				/* This meta key was not serialized up to now */
				size_t metanamesize = ckdb::keyGetNameSize(meta);
				size_t metavaluesize = ckdb::keyGetValueSize(meta);

				os << "keyMeta " << metanamesize
				   << " " << metavaluesize << std::endl;
				os.write (ckdb::keyName(meta), metanamesize);
				os.write (static_cast<const char*>(ckdb::keyValue(meta)), metavaluesize);
				os << std::endl;

				std::stringstream ssv;
				ssv << namesize << " " << metanamesize << std::endl;
				ssv.write(ckdb::keyName(cur), namesize);
				ssv.write (ckdb::keyName(meta), metanamesize);
				ckdb::keySetRaw(search, ssv.str().c_str(), ssv.str().size());

				ksAppendKey(metacopies, search);
			} else {
				/* Meta key already serialized, write out a reference to it */
				keyDel (search);

				os << "keyCopyMeta ";
				os.write(static_cast<const char*>(ckdb::keyValue(ret)), ckdb::keyGetValueSize(ret));
				os << std::endl;
			}
		}
		os << "keyEnd" << std::endl;
	}
	os << "ksEnd" << std::endl;

	ksDel (metacopies);

	return 1;
}

int unserialize(std::istream &is, ckdb::Key *errorKey, ckdb::KeySet *ks)
{
	ckdb::Key *cur = 0;

	std::vector<char> namebuffer(4048);
	std::vector<char> valuebuffer(4048);
	std::string line;
	std::string command;
	size_t nrKeys;
	size_t namesize;
	size_t valuesize;

	while(std::getline (is, line))
	{
		std::stringstream ss (line);
		ss >> command;

		if (command == "kdbOpen")
		{
			std::string version;
			ss >> version;
			if (version != "1")
			{
				ELEKTRA_SET_ERROR (50, errorKey, version.c_str());
				return -1;
			}
		}
		else if (command == "ksNew")
		{
			ss >> nrKeys;

			ksClear(ks);
		}
		else if (command == "keyNew")
		{
			cur = ckdb::keyNew(0);

			ss >> namesize;
			ss >> valuesize;

			if (namesize > namebuffer.size()) namebuffer.resize(namesize);
			is.read(&namebuffer[0], namesize);
			namebuffer[namesize] = 0;
			ckdb::keySetName(cur, &namebuffer[0]);

			if (valuesize > valuebuffer.size()) valuebuffer.resize(valuesize);
			is.read(&valuebuffer[0], valuesize);
			ckdb::keySetRaw (cur, &valuebuffer[0], valuesize);
			std::getline (is, line);
		}
		else if (command == "keyMeta")
		{
			ss >> namesize;
			ss >> valuesize;

			if (namesize > namebuffer.size()) namebuffer.resize(namesize);
			is.read(&namebuffer[0], namesize);
			namebuffer[namesize] = 0;

			if (valuesize > valuebuffer.size()) valuebuffer.resize(valuesize);
			is.read(&valuebuffer[0], valuesize);

			keySetMeta (cur, &namebuffer[0], &valuebuffer[0]);
			std::getline (is, line);
		}
		else if (command == "keyCopyMeta")
		{
			ss >> namesize;
			ss >> valuesize;

			if (namesize > namebuffer.size()) namebuffer.resize(namesize);
			is.read(&namebuffer[0], namesize);
			namebuffer[namesize] = 0;

			if (valuesize > valuebuffer.size()) valuebuffer.resize(valuesize);
			is.read(&valuebuffer[0], valuesize);

			ckdb::Key * search = ckdb::ksLookupByName(ks, &namebuffer[0], 0);
			ckdb::keyCopyMeta(cur, search, &valuebuffer[0]);
			std::getline (is, line);
		}
		else if (command == "keyEnd")
		{
			ckdb::keyClearSync(cur);
			ckdb::ksAppendKey(ks, cur);
			cur = 0;
		}
		else if (command == "ksEnd")
		{
			break;
		} else {
			ELEKTRA_SET_ERROR (49, errorKey, command.c_str());
			return -1;
		}
	}
	return 1;
}

} // namespace dump


extern "C" {

int kdbGet_dump(ckdb::Plugin *, ckdb::KeySet *returned, ckdb::Key *parentKey)
{
	Key *root = ckdb::keyNew("system/elektra/modules/dump", KEY_END);
	if (keyRel(root, parentKey) >= 0)
	{
		keyDel (root);
		void (*get) (void) = (void (*) (void)) kdbGet_dump;
		void (*set) (void) = (void (*) (void)) kdbSet_dump;
		void (*serialize) (void) = (void (*) (void)) dump::serialize;
		void (*unserialize) (void) = (void (*) (void)) dump::unserialize;
		KeySet *n = ksNew(50,
			keyNew ("system/elektra/modules/dump",
				KEY_VALUE, "dump plugin waits for your orders", KEY_END),
			keyNew ("system/elektra/modules/dump/exports", KEY_END),
			keyNew ("system/elektra/modules/dump/exports/get",
				KEY_SIZE, sizeof (get),
				KEY_BINARY,
				KEY_VALUE, &get, KEY_END),
			keyNew ("system/elektra/modules/dump/exports/set",
				KEY_SIZE, sizeof (set),
				KEY_BINARY,
				KEY_VALUE, &set, KEY_END),
			keyNew ("system/elektra/modules/dump/exports/serialize",
				KEY_SIZE, sizeof (serialize),
				KEY_BINARY,
				KEY_VALUE, &serialize, KEY_END),
			keyNew ("system/elektra/modules/dump/exports/unserialize",
				KEY_SIZE, sizeof (unserialize),
				KEY_BINARY,
				KEY_VALUE, &unserialize, KEY_END),
			keyNew ("system/elektra/modules/dump/infos",
				KEY_VALUE, "All information you want to know", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/author",
				KEY_VALUE, "Markus Raab <elektra@markus-raab.org>", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/licence",
				KEY_VALUE, "BSD", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/description",
				KEY_VALUE, "Dumps complete Elektra Semantics", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/provides",
				KEY_VALUE, "storage", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/placements",
				KEY_VALUE, "getstorage setstorage", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/needs",
				KEY_VALUE, "", KEY_END),
			keyNew ("system/elektra/modules/dump/infos/version",
				KEY_VALUE, PLUGINVERSION, KEY_END),
			KS_END);
		ksAppend(returned, n);
		ksDel (n);
		return 1;
	}
	keyDel (root);
	std::ifstream ofs(keyString(parentKey), std::ios::binary);
	if (!ofs.is_open()) return 0;

	return dump::unserialize (ofs, parentKey, returned);
}

int kdbSet_dump(ckdb::Plugin *, ckdb::KeySet *returned, ckdb::Key *parentKey)
{
	std::ofstream ofs(keyString(parentKey), std::ios::binary);
	if (!ofs.is_open())
	{
		ELEKTRA_SET_ERROR (9, parentKey, "file is not open in dump");
		return -1;
	}

	return dump::serialize (ofs, parentKey, returned);
}

ckdb::Plugin *ELEKTRA_PLUGIN_EXPORT(dump)
{
	return elektraPluginExport("dump",
		ELEKTRA_PLUGIN_GET,		&kdbGet_dump,
		ELEKTRA_PLUGIN_SET,		&kdbSet_dump,
		ELEKTRA_PLUGIN_END);
}

} // extern C

