#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

namespace BDD {
	class BDD;
}

namespace Clone {
	using std::string;
	using std::shared_ptr;
	using std::unordered_map;

	class NF {
	private:
		const string id;
		const string path;
		shared_ptr<const BDD::BDD> bdd = nullptr;
	public:
		NF(const string &id, const string &path);
		~NF();

		inline const string &get_id() const {
			return id;
		}

		inline const string &get_path() const {
			return path;
		}

		inline const shared_ptr<const BDD::BDD> &get_bdd() const {
			return bdd;
		}

		inline void set_bdd(shared_ptr<const BDD::BDD> bdd) {
			this->bdd = bdd;
		}

		void print() const;
	};

	typedef shared_ptr<NF> NFPtr;
	typedef unordered_map<string, NFPtr> NFMap;
}
