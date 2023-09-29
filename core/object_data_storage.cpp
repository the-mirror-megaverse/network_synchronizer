#include "object_data_storage.h"

#include "core/error/error_macros.h"
#include "core/os/memory.h"
#include "modules/network_synchronizer/net_utilities.h"
#include "modules/network_synchronizer/scene_synchronizer.h"

NS_NAMESPACE_BEGIN

ObjectDataStorage::ObjectDataStorage(SceneSynchronizerBase &p_sync) :
		sync(p_sync) {
}

ObjectDataStorage::~ObjectDataStorage() {
	for (auto od : objects_data) {
		if (od) {
			delete od;
		}
	}

	free_local_indices.clear();
	objects_data.clear();
	objects_data_organized_by_netid.clear();
	objects_data_controllers.clear();
}

ObjectData *ObjectDataStorage::allocate_object_data() {
	ObjectData *od = new ObjectData(*this);

	if (free_local_indices.empty()) {
		od->local_id.id = objects_data.size();
		objects_data.push_back(od);
	} else {
		od->local_id.id = free_local_indices.back().id;
		free_local_indices.pop_back();
		CRASH_COND(objects_data.size() <= od->local_id.id);
		CRASH_COND(objects_data[od->local_id.id] != nullptr);
		objects_data[od->local_id.id] = od;
	}

	CRASH_COND(objects_data[od->local_id.id] != od);

	return od;
}

void ObjectDataStorage::deallocate_object_data(ObjectData &p_object_data) {
	const ObjectLocalId local_id = p_object_data.local_id;
	const ObjectNetId net_id = p_object_data.net_id;

	// The allocate function guarantee the validity of this check.
	CRASH_COND(objects_data[local_id.id] != (&p_object_data));
	objects_data[local_id.id] = nullptr;

	if (objects_data_organized_by_netid.size() > net_id) {
		CRASH_COND(objects_data_organized_by_netid[net_id] != (&p_object_data));
		objects_data_organized_by_netid[net_id] = nullptr;
	}

	// Clear the controller pointer.
	if (p_object_data.controller) {
		p_object_data.controller = nullptr;
		notify_set_controller(p_object_data);
	}

	delete (&p_object_data);

	free_local_indices.push_back(local_id);
}

void ObjectDataStorage::object_set_net_id(ObjectData &p_object_data, ObjectNetId p_new_id) {
	if (p_object_data.net_id == p_new_id) {
		return;
	}

	if (objects_data_organized_by_netid.size() > p_object_data.net_id) {
		objects_data_organized_by_netid[p_object_data.net_id] = nullptr;
	}

	p_object_data.net_id = ID_NONE;

	if (p_new_id == ID_NONE) {
		sync.notify_object_data_net_id_changed(p_object_data);
		return;
	}

	if (objects_data_organized_by_netid.size() > p_new_id) {
		if (objects_data_organized_by_netid[p_new_id] && objects_data_organized_by_netid[p_new_id] != (&p_object_data)) {
			ERR_PRINT("[NET] The object `" + String(p_object_data.object_name.c_str()) + "` was associated with to a new NetId that was used by `" + String(objects_data_organized_by_netid[p_new_id]->object_name.c_str()) + "`. THIS IS NOT SUPPOSED TO HAPPEN.");
		}
	} else {
		// Expand the array
		const uint32_t new_size = p_new_id + 1;
		const uint32_t old_size = objects_data_organized_by_netid.size();
		objects_data_organized_by_netid.resize(new_size);
		// Set the new pointers to nullptr.
		memset(objects_data_organized_by_netid.data() + old_size, 0, sizeof(void *) * (new_size - old_size));
	}

	objects_data_organized_by_netid[p_new_id] = &p_object_data;
	p_object_data.net_id = p_new_id;
	sync.notify_object_data_net_id_changed(p_object_data);
}

NS::ObjectData *ObjectDataStorage::find_object_data(ObjectHandle p_handle) {
	for (uint32_t i = 0; i < objects_data.size(); i += 1) {
		if (objects_data[i] == nullptr) {
			continue;
		}
		if (objects_data[i]->app_object_handle == p_handle) {
			return objects_data[i];
		}
	}
	return nullptr;
}

const NS::ObjectData *ObjectDataStorage::find_object_data(ObjectHandle p_handle) const {
	for (uint32_t i = 0; i < objects_data.size(); i += 1) {
		if (objects_data[i] == nullptr) {
			continue;
		}
		if (objects_data[i]->app_object_handle == p_handle) {
			return objects_data[i];
		}
	}
	return nullptr;
}

ObjectData *ObjectDataStorage::find_object_data(NetworkedControllerBase &p_controller) {
	for (auto od : objects_data_controllers) {
		if (od->controller == (&p_controller)) {
			return od;
		}
	}
	return nullptr;
}

const ObjectData *ObjectDataStorage::find_object_data(const NetworkedControllerBase &p_controller) const {
	for (auto od : objects_data_controllers) {
		if (od->controller == (&p_controller)) {
			return od;
		}
	}
	return nullptr;
}

NS::ObjectData *ObjectDataStorage::get_object_data(ObjectNetId p_net_id, bool p_expected) {
	if (p_expected) {
		ERR_FAIL_UNSIGNED_INDEX_V(p_net_id, objects_data_organized_by_netid.size(), nullptr);
	} else {
		if (objects_data_organized_by_netid.size() <= p_net_id) {
			return nullptr;
		}
	}

	return objects_data_organized_by_netid[p_net_id];
}

const NS::ObjectData *ObjectDataStorage::get_object_data(ObjectNetId p_net_id, bool p_expected) const {
	if (p_expected) {
		ERR_FAIL_UNSIGNED_INDEX_V(p_net_id, objects_data_organized_by_netid.size(), nullptr);
	} else {
		if (objects_data_organized_by_netid.size() <= p_net_id) {
			return nullptr;
		}
	}

	return objects_data_organized_by_netid[p_net_id];
}

NS::ObjectData *ObjectDataStorage::get_object_data(ObjectLocalId p_handle, bool p_expected) {
	if (p_expected) {
		ERR_FAIL_UNSIGNED_INDEX_V(p_handle.id, objects_data_organized_by_netid.size(), nullptr);
	} else {
		if (objects_data_organized_by_netid.size() <= p_handle.id) {
			return nullptr;
		}
	}

	return objects_data[p_handle.id];
}

const NS::ObjectData *ObjectDataStorage::get_object_data(ObjectLocalId p_handle, bool p_expected) const {
	if (p_expected) {
		ERR_FAIL_UNSIGNED_INDEX_V(p_handle.id, objects_data_organized_by_netid.size(), nullptr);
	} else {
		if (objects_data_organized_by_netid.size() <= p_handle.id) {
			return nullptr;
		}
	}

	return objects_data[p_handle.id];
}

void ObjectDataStorage::reserve_net_ids(int p_count) {
	objects_data_organized_by_netid.reserve(p_count);
}

const std::vector<ObjectData *> &ObjectDataStorage::get_objects_data() const {
	return objects_data;
}

const std::vector<ObjectData *> &ObjectDataStorage::get_controllers_objects_data() const {
	return objects_data_controllers;
}

const std::vector<ObjectData *> &ObjectDataStorage::get_sorted_objects_data() const {
	return objects_data_organized_by_netid;
}

ObjectNetId ObjectDataStorage::generate_net_id() const {
	int i = 0;
	for (auto od : objects_data_organized_by_netid) {
		if (!od) {
			// This position is empty, can be used as NetId.
			return i;
		}
		i++;
	}

	// Create a new NetId.
	return objects_data_organized_by_netid.size();
}

bool ObjectDataStorage::is_empty() const {
	for (auto od : objects_data) {
		if (od) {
			return false;
		}
	}

	return true;
}

void ObjectDataStorage::notify_set_controller(ObjectData &p_object) {
	auto it = std::find(objects_data_controllers.begin(), objects_data_controllers.end(), &p_object);
	if (p_object.controller) {
		if (it == objects_data_controllers.end()) {
			objects_data_controllers.push_back(&p_object);
		}
	} else {
		if (it != objects_data_controllers.end()) {
			objects_data_controllers.erase(it);
		}
	}
}

NS_NAMESPACE_END