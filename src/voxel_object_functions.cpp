/*
	Copyright (C) 2003-2014 by David White <davewx7@gmail.com>
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	   1. The origin of this software must not be misrepresented; you must not
	   claim that you wrote the original software. If you use this software
	   in a product, an acknowledgement in the product documentation would be
	   appreciated but is not required.

	   2. Altered source versions must be plainly marked as such, and must not be
	   misrepresented as being the original software.

	   3. This notice may not be removed or altered from any source
	   distribution.
*/

/* XXX -- needs re-write

#include "formula_callable_definition.hpp"
#include "formula_function_registry.hpp"
#include "isoworld.hpp"
#include "object_events.hpp"
#include "user_voxel_object.hpp"
#include "voxel_object.hpp"
#include "voxel_object_functions.hpp"

void voxel_object_command_callable::setExpression(const game_logic::FormulaExpression* expr)
{
	expr_ = expr;
	expr_holder_.reset(expr);
}


void voxel_object_command_callable::runCommand(voxel::world& lvl, voxel::user_voxel_object& obj) const
{
	if(expr_) {
		try {
			fatal_assert_scope scope;
			execute(lvl, obj);
		} catch(fatal_assert_failure_exception& e) {
			ASSERT_FATAL(e.msg << "\nERROR ENCOUNTERED WHILE RUNNING COMMAND GENERATED BY THIS EXPRESSION:\n" << expr_->debugPinpointLocation());
		}
	} else {
		execute(lvl, obj);
	}
}

using namespace game_logic;

namespace {

const std::string FunctionModule = "voxel_object";

class schedule_command : public voxel_object_command_callable 
{
public:
	schedule_command(int cycles, variant cmd) : cycles_(cycles), cmd_(cmd)
	{}

	virtual void execute(voxel::world& lvl, voxel::user_voxel_object& ob) const 
	{
		ob.addScheduledCommand(cycles_, cmd_);
	}
private:
	int cycles_;
	variant cmd_;
};

FUNCTION_DEF(schedule, 2, 2, "schedule(int cycles_in_future, list of commands): schedules the given list of commands to be run on the current object the given number of cycles in the future. Note that the object must be valid (not destroyed) and still present in the level for the commands to be run.")
	schedule_command* cmd = new schedule_command(EVAL_ARG(0).as_int(),EVAL_ARG(1));
	cmd->setExpression(this);
	return variant(cmd);
FUNCTION_ARGS_DEF
	ARG_TYPE("int")
	ARG_TYPE("commands")
	RETURN_TYPE("commands")
END_FUNCTION_DEF(schedule)

static int event_depth = 0;
struct event_depth_scope 
{
	event_depth_scope() 
	{ 
		++event_depth;
	}
	~event_depth_scope() 
	{ 
		--event_depth;
	}
};

class fire_event_command : public voxel_object_command_callable 
{
	const voxel::UserVoxelObjectPtr target_;
	const std::string event_;
	const ConstFormulaCallablePtr callable_;
public:
	fire_event_command(voxel::UserVoxelObjectPtr target, const std::string& event, ConstFormulaCallablePtr callable)
	  : target_(target), event_(event), callable_(callable)
	{}

	virtual void execute(voxel::world& lvl, voxel::user_voxel_object& ob) const
	{
		ASSERT_LOG(event_depth < 1000, "INFINITE (or too deep?) RECURSION FOR EVENT " << event_);
		event_depth_scope scope;
		voxel::user_voxel_object* e = target_ ? target_.get() : &ob;
		e->handleEvent(event_, callable_.get());
	}
};

FUNCTION_DEF(fire_event, 1, 3, "fire_event((optional) object target, string id, (optional)callable arg): fires the event with the given id. Targets the current object by default, or target if given. Sends arg as the event argument if given")
	voxel::UserVoxelObjectPtr target;
	std::string event;
	ConstFormulaCallablePtr callable;
	variant arg_value;

	if(args().size() == 3) {
		variant v1 = args()[0]->evaluate(variables);
		if(v1.is_null()) {
			return variant();
		}

		target = v1.convert_to<voxel::user_voxel_object>();
		event = args()[1]->evaluate(variables).as_string();

		arg_value = args()[2]->evaluate(variables);
	} else if(args().size() == 2) {
		variant v1 = args()[0]->evaluate(variables);
		if(v1.is_null()) {
			return variant();
		}

		variant v2 = args()[1]->evaluate(variables);
		if(v1.is_string()) {
			event = v1.as_string();
			arg_value = v2;
		} else {
			target = v1.convert_to<voxel::user_voxel_object>();
			event = v2.as_string();
		}
	} else {
		event = args()[0]->evaluate(variables).as_string();
	}

	variant_type_ptr arg_type = get_object_event_arg_type(get_object_event_id(event));
	if(arg_type) {
		ASSERT_LOG(arg_type->match(arg_value), "Calling fire_event, arg type does not match. Expected " << arg_type->to_string() << " found " << arg_value.write_json() << " which is a " << get_variant_type_from_value(arg_value)->to_string());
	}

	if(arg_value.is_null() == false) {
		callable = map_into_callable(arg_value);
	}

	fire_event_command* cmd = (new fire_event_command(target, event, callable));
	cmd->setExpression(this);
	return variant(cmd);
FUNCTION_ARGS_DEF
	ARG_TYPE("object|string")
	ARG_TYPE("any")
	ARG_TYPE("any")
RETURN_TYPE("commands")
END_FUNCTION_DEF(fire_event)


//class spawn_voxel_command : public voxel_object_command_callable
//{
//public:
//	spawn_voxel_command(voxel::UserVoxelObjectPtr obj, variant instantiation_commands)
//	  : obj_(obj), instantiation_commands_(instantiation_commands)
//	{}
//	virtual void execute(voxel::world& lvl, voxel::voxel_object& ob) const {
//
//		lvl.add_object(obj_);
//
//		obj_->executeCommand(instantiation_commands_);
//	}
//private:
//	voxel::UserVoxelObjectPtr obj_;
//	variant instantiation_commands_;
//};
//
//FUNCTION_DEF(spawn_voxel, 4, 6, "spawn_voxel(string type_id, decimal x, decimal y, decimal z, (optional) properties, (optional) list of commands cmd): will create a new object of type given by type_id with the given midpoint and facing. Immediately after creation the object will have any commands given by cmd executed on it. The child object will have the spawned event sent to it, and the parent object will have the child_spawned event sent to it.")
//
//	formula::failIfStaticContext();
//
//	const std::string type = EVAL_ARG(0).as_string();
//	const float x = float(EVAL_ARG(1).as_decimal().as_float());
//	const float y = float(EVAL_ARG(2).as_decimal().as_float());
//	const float z = float(EVAL_ARG(3).as_decimal().as_float());
//
//	variant arg4 = EVAL_ARG(4);
//
//	voxel::UserVoxelObjectPtr obj(new voxel::user_voxel_object(type, x, y, z));
//
//	variant commands;
//	spawn_voxel_command* cmd = (new spawn_voxel_command(obj, commands));
//	cmd->setExpression(this);
//	return variant(cmd);
//FUNCTION_ARGS_DEF
//	//ASSERT_LOG(false, "spawn() not supported in strict mode " << debugPinpointLocation());
//	ARG_TYPE("string")
//	ARG_TYPE("decimal")
//	ARG_TYPE("decimal")
//	ARG_TYPE("decimal")
//	ARG_TYPE("map")
//	ARG_TYPE("commands")
//
//	variant v;
//	if(args()[0]->canReduceToVariant(v) && v.is_string()) {
//		game_logic::FormulaCallableDefinitionPtr type_def = CustomObjectType::getDefinition(v.as_string());
//		const CustomObjectCallable* type = dynamic_cast<const CustomObjectCallable*>(type_def.get());
//		ASSERT_LOG(type, "Illegal object type: " << v.as_string() << " " << debugPinpointLocation());
//
//		if(args().size() > 3) {
//			variant_type_ptr map_type = args()[3]->queryVariantType();
//			assert(map_type);
//
//			const std::map<variant, variant_type_ptr>* props = map_type->is_specific_map();
//			if(props) {
//				foreach(int slot, type->slots_requiring_initialization()) {
//					const std::string& prop_id = type->getEntry(slot)->id;
//					ASSERT_LOG(props->count(variant(prop_id)), "Must initialize " << v.as_string() << "." << prop_id << " " << debugPinpointLocation());
//				}
//
//				for(std::map<variant,variant_type_ptr>::const_iterator itor = props->begin(); itor != props->end(); ++itor) {
//					const int slot = type->getSlot(itor->first.as_string());
//					ASSERT_LOG(slot >= 0, "Unknown property " << v.as_string() << "." << itor->first.as_string() << " " << debugPinpointLocation());
//
//					const FormulaCallableDefinition::Entry& entry = *type->getEntry(slot);
//					ASSERT_LOG(variant_types_compatible(entry.getWriteType(), itor->second), "Initializing property " << v.as_string() << "." << itor->first.as_string() << " with type " << itor->second->to_string() << " when " << entry.getWriteType()->to_string() << " is expected " << debugPinpointLocation());
//				}
//			}
//		}
//
//		ASSERT_LOG(type->slots_requiring_initialization().empty() || args().size() > 3 && args()[3]->queryVariantType()->is_map_of().first, "Illegal spawn of " << v.as_string() << " property " << type->getEntry(type->slots_requiring_initialization()[0])->id << " requires initialization " << debugPinpointLocation());
//	}
//RETURN_TYPE("commands")
//END_FUNCTION_DEF(spawn_voxel)

class voxel_object_FunctionSymbolTable : public FunctionSymbolTable
{
public:
	ExpressionPtr createFunction(const std::string& fn,
		const std::vector<ExpressionPtr>& args,
		ConstFormulaCallableDefinitionPtr callable_def) const
	{
		const std::map<std::string, function_creator*>& creators = get_function_creators(FunctionModule);
		std::map<std::string, function_creator*>::const_iterator i = creators.find(fn);
		if(i != creators.end()) {
			return ExpressionPtr(i->second->create(args));
		}

		return FunctionSymbolTable::createFunction(fn, args, callable_def);
	}

};

}

FunctionSymbolTable& get_voxel_object_functions_symbol_table()
{
	static voxel_object_FunctionSymbolTable table;
	return table;
}
*/
