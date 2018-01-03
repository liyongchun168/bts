/*
 * Copyright (C) 2010-2017 Canonical
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "fwts.h"

#if defined(FWTS_HAS_ACPI)

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

/* acpica headers */
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "acpi.h"
#pragma GCC diagnostic error "-Wunused-parameter"
#include "fwts_acpi_object_eval.h"

typedef struct {
	const ACPI_STATUS status;
	const fwts_log_level level;
	const char *error_type;
	const char *error_text;
	const char *advice;
} acpi_eval_error;

static const acpi_eval_error errors[] = {
	/* ACPI_STATUS			fwts_log_level		error_type		error_text */
	{ AE_ERROR,			LOG_LEVEL_HIGH,		"AEError",		"Unspecified error",
		"An internal unspecified ACPICA execution error has occurred."
	},
	{ AE_NO_ACPI_TABLES,		LOG_LEVEL_HIGH,		"AENoACPITables",	"No ACPI tables",
		"No ACPI tables could be found.  Normally this indicates the tables could not be loaded "
		"from firmware or a provided firmware dump file."
	},
	{ AE_NO_NAMESPACE,		LOG_LEVEL_HIGH,		"AENoNamespace",	"No namespace",
		"An ACPI namespace was not loaded. This happens if an ACPI root node could not be found."
	},
	{ AE_NO_MEMORY,			LOG_LEVEL_CRITICAL,	"AENoMemory",		"Out of memory",
		"No more memory could be allocated from the heap."
	},
	{ AE_NOT_FOUND,			LOG_LEVEL_CRITICAL,	"AENotFound",		"Not found",
		"An ACPI object or entity was requested but could not be found. Running fwts with the "
		"--acpica=slack option may work around this issue."
	},
	{ AE_NOT_EXIST,			LOG_LEVEL_CRITICAL,	"AENotExist",		"Not exist",
		"An ACPI object or entity was requested but does not exist.",
	},
	{ AE_ALREADY_EXISTS,		LOG_LEVEL_HIGH,		"AEAlreadyExists",	"Already exists",
		"An ACPI object or entity already exists.  Sometimes this occurs when code is not "
		"serialized and it has been created a second time by another thread. ACPICA can detect "
		"these errors and enable auto serialization to try to work around this error. "
	},
	{ AE_TYPE,			LOG_LEVEL_CRITICAL,	"AETtype",		"Type",
		"The type of an object is incorrect and does not match the expected type. ACPI will "
		"generally abort execution of the AML op-code that causes this error."
	},
	{ AE_NULL_OBJECT,		LOG_LEVEL_CRITICAL,	"AENullObject",		"Null object",
		"An ACPI object is NULL when it was expected to be non-NULL."
	},
	{ AE_NULL_ENTRY,		LOG_LEVEL_CRITICAL,	"AENullEntry",		"Null entry",
		"The requested ACPI object did not exist.  This can occur when a root entry is "
		"NULL."
	},
	{ AE_BUFFER_OVERFLOW,		LOG_LEVEL_CRITICAL,	"AEBufferOverflow",	"Buffer overflow",
		"An ACPI buffer that was provided was too small to contain the data to be copied "
		"into it. This generally indicates an AML bug."
	},
	{ AE_STACK_OVERFLOW,		LOG_LEVEL_CRITICAL,	"AEStackOverflow",	"Stack overflow",
		"An ACPI method call failed because the call nesting was too deep and the "
		"internal APCI execution stack overflowed. This generally occurs with AML that "
		"may be recursing too deeply."
	},
	{ AE_STACK_UNDERFLOW,		LOG_LEVEL_CRITICAL,	"AEStackUnderflow",	"Stack underflow",
		"An ACPI method call failed because the internal ACPI execution stack underflowed. "
		"This occurred while walking the object stack and popping off an object from an empty stack."
	},
	{ AE_NOT_IMPLEMENTED,		LOG_LEVEL_HIGH,		"AENotImplemented",	"Not implemented",
		"The ACPICA execution engine has not implemented a feature."
	},
	{ AE_SUPPORT,			LOG_LEVEL_HIGH,		"AESupport",		"Support",
		"Normally this occurs when AML attempts to perform an action that is not supported, "
		"such as converting objects in an unsupported way or attempting to access memory using "
		"an unsupported address space type."
	},
	{ AE_LIMIT,			LOG_LEVEL_CRITICAL,	"AELimit",		"Limit",
		"A predefined limit was reached, for example, attempting to access an I/O port above the "
		"allowed 64K limit or a semaphore handle is out of range."
	},
	{ AE_TIME,			LOG_LEVEL_HIGH,		"AETime",		"Timeout",
		"A timeout has been reached, for example a semaphore has timed out."
	},
	{ AE_ACQUIRE_DEADLOCK,		LOG_LEVEL_CRITICAL,	"AEAcqDeadlock",	"Acquire deadlock",
		"A mutex acquire was aborted because a potential deadlock was detected."
	},
	{ AE_RELEASE_DEADLOCK,		LOG_LEVEL_CRITICAL,	"AERelDeadlock",	"Release deadlock",
		"A mutex release was aborted because a potential deadlock was detected."
	},
	{ AE_NOT_ACQUIRED,		LOG_LEVEL_CRITICAL,	"AENotAcq",		"Not acquired",
		"An attempt to release a mutex or an ACPI global lock failed because it "
		"had not previously acquired. This normally indicates an AML programming bug."
	},
	{ AE_ALREADY_ACQUIRED,		LOG_LEVEL_CRITICAL,	"AEAlreadyAcq",		"Already acquired",
		"An attempt to re-acquire mutex or an ACPI global lock failed because it "
		"had not previously acquired. This normally indicates an AML programming bug."
	},
	{ AE_NO_HARDWARE_RESPONSE,	LOG_LEVEL_HIGH,		"AENoHWResponse",	"No hardware response",
		"ACPI gave up waiting for hardware to respond. For example, a SMI_CMD failed or "
		"the platform does not have an ACPI global lock defined or an ACPI event could "
		"not be enabled."
	},
	{ AE_NO_GLOBAL_LOCK,		LOG_LEVEL_HIGH,		"AENoGlobalLock",	"No global lock",
		"The platform does not define an ACPI global lock in the FACS."
	},
	{ AE_ABORT_METHOD,		LOG_LEVEL_CRITICAL,	"AEAbortMethod",	"Abort method",
		"Execution of an ACPI method was aborted because the internal ACPICA AcpiGbl_AbortMethod "
		"flag was set to true.  This should not occur."
	},
	{ AE_SAME_HANDLER,		LOG_LEVEL_HIGH,		"AESameHandler",	"Same handler",
		"An attempt was made to install the same handler that is already installed."
	},
	{ AE_NO_HANDLER,		LOG_LEVEL_CRITICAL,	"AENoHandler",		"No handler",
		"A handler for a General Purpose Event operation was not installed."
	},
	{ AE_OWNER_ID_LIMIT,		LOG_LEVEL_HIGH,		"AEOwnerIDLimit",	"Owner ID limit",
		"A maximum of 255 OwnerIds have been used. There are no more Owner IDs available for "
		"ACPI tables or control methods."
	},

	{ AE_BAD_PARAMETER,		LOG_LEVEL_HIGH,		"AEBadParam",		"Args: Bad paramater",
		"A parameter passed is out of range or invalid."
	},
	{ AE_BAD_CHARACTER,		LOG_LEVEL_HIGH,		"AEBadChar",		"Args: Bad character",
		"An invalid character was found in an ACPI name."
	},
	{ AE_BAD_PATHNAME,		LOG_LEVEL_HIGH,		"AEBadPathname",	"Args: Bad pathname",
		"An internal AML namestring could not be built because of an invalid pathname."
	},
	{ AE_BAD_DATA,			LOG_LEVEL_HIGH,		"AEBadData",		"Args: Bad data",
		"A buffer or package contained some incorrect data."
	},
	{ AE_BAD_HEX_CONSTANT,		LOG_LEVEL_HIGH,		"AEBadHexConst",	"Args: Bad hex constant",
		"An invalid character was found in a hexadecimal constant."
	},
	{ AE_BAD_OCTAL_CONSTANT,	LOG_LEVEL_HIGH,		"AEBadOctConst",	"Args: Bad octal constant",
		"An invalid character was found in an octal constant."
	},
	{ AE_BAD_DECIMAL_CONSTANT,	LOG_LEVEL_HIGH,		"AEBadDecConst",	"Args: Bad decimal constant",
		"An invalid character was found in a decimal constant."
	},
	{ AE_MISSING_ARGUMENTS,		LOG_LEVEL_CRITICAL,	"AEMissingArgs",	"Args: Missing arguments",
		"Too few arguments were passed into an ACPI control method."
	},
	{ AE_BAD_ADDRESS,		LOG_LEVEL_CRITICAL,	"AEBadAddr",		"Args: Bad address",
		"An illegal null I/O address was referenced by the AML."
	},

	{ AE_AML_BAD_OPCODE, 		LOG_LEVEL_CRITICAL,	"AEAMLBadOpCode",	"Bad AML opcode",
		"An invalid AML opcode encountered during AML execution."
	},
	{ AE_AML_NO_OPERAND, 		LOG_LEVEL_HIGH,		"AEAMLNoOperand",	"Missing operand",
		"An AML opcode was missing a required operand."
	},
	{ AE_AML_OPERAND_TYPE, 		LOG_LEVEL_HIGH,		"AEAMLOperandType",	"Incorrect operand type",
		"An AML opcode had an operand of an incorrect type."
	},
	{ AE_AML_OPERAND_VALUE, 	LOG_LEVEL_HIGH,		"AEAMLOperandValue",	"Incorrect operand value",
		"An AML opcode has an operand of an inappropriate or invalid value.",
	},
	{ AE_AML_UNINITIALIZED_LOCAL, 	LOG_LEVEL_HIGH,		"AEAMLUninitLocal",	"Uninitialized local variable",
		"A method attempted to use a local variable that was not initialized."
	},
	{ AE_AML_UNINITIALIZED_ARG, 	LOG_LEVEL_HIGH,		"AEAMLUninitArg",	"Uninitialized argument",
		"A method attempted to use an uninitialized argument."
	},
	{ AE_AML_UNINITIALIZED_ELEMENT, LOG_LEVEL_HIGH,		"AEAMLUninitElement",	"Uninitialized element",
		"A method attempted to use an empty element in an ACPI package."
	},
	{ AE_AML_NUMERIC_OVERFLOW, 	LOG_LEVEL_HIGH,		"AEAMLNumericOverflow",	"Numeric overflow",
		"A numeric overflow occurred, for example when coverting a BCD value."
	},
	{ AE_AML_REGION_LIMIT, 		LOG_LEVEL_CRITICAL,	"AEAMLRegionLimit",	"Region limit",
		"An attempt to access beyond the end of an ACPI Operation Region occurred."
	},
	{ AE_AML_BUFFER_LIMIT, 		LOG_LEVEL_CRITICAL,	"AEAMLBufferLimit",	"Buffer limit",
		"An attempt to access beyond the end of an ACPI buffer occurred."
	},
	{ AE_AML_PACKAGE_LIMIT, 	LOG_LEVEL_CRITICAL,	"AEAMLPackgeLimit",	"Package limit",
		"An attempt to access beyond the endof an ACPI package occurred."
	},
	{ AE_AML_DIVIDE_BY_ZERO, 	LOG_LEVEL_CRITICAL,	"AEAMLDivideByZero",	"Division by zero",
		"A division by zero error occurred while excuting the AML Divide op-code"
	},
	{ AE_AML_NAME_NOT_FOUND, 	LOG_LEVEL_HIGH,		"AEAMLNameNotFound",	"Name not found",
		"An ACPI named reference could not be resolved."
	},
	{ AE_AML_INTERNAL, 		LOG_LEVEL_HIGH,		"AEAMLInternal",	"Internal ACPICA execution engine error",
		"An internal error occurred in the ACPICA execution interpreter.  A bug report should "
		"be filed against fwts so that this can be resolved."
	},
	{ AE_AML_INVALID_SPACE_ID, 	LOG_LEVEL_HIGH,		"AEAMLInvalidSpaceID",	"Invalid space ID error",
		"An invalid ACPI Operation Region Space ID has been encountered."
	},
	{ AE_AML_STRING_LIMIT, 		LOG_LEVEL_HIGH,		"AEAMLStringLimit",	"String limit",
		"A string more than 200 characters long has been used."
	},
	{ AE_AML_NO_RETURN_VALUE, 	LOG_LEVEL_HIGH,		"AEAMLNoReturnValue",	"No return value",
		"A method did not return the required value that was expected."
	},
	{ AE_AML_METHOD_LIMIT, 		LOG_LEVEL_HIGH,		"AEAMLMethodLimit",	"Method limit",
		"A control method reached a limit of 255 concurrent thread executions and hit the ACPICA "
		"re-entrancy limit."
	},
	{ AE_AML_NOT_OWNER, 		LOG_LEVEL_HIGH,		"AEAMLNotOwner",	"Not owner",
		"A thread attempted to release a mutex that it did not own.",
	},
	{ AE_AML_MUTEX_ORDER, 		LOG_LEVEL_HIGH,		"AEAMLMutexOrder",	"Mutex order",
		"Deadlock prevention has detected that the current mutex sync level is too large."
	},
	{ AE_AML_MUTEX_NOT_ACQUIRED, 	LOG_LEVEL_HIGH,		"AEAMLMutexNotAcq",	"Mutux not acquired",
		"An attempt was made to release a mutex that had not been acquired."
	},
	{ AE_AML_INVALID_RESOURCE_TYPE, LOG_LEVEL_HIGH,		"AEAMLInvResourceType", "Invalid resource type",
		"An ACPI resource list contained an invalid resource type."
	},
	{ AE_AML_INVALID_INDEX, 	LOG_LEVEL_HIGH,		"AEAMLInvIndex",	"Invalid index",
		"An attempt to get a node with an index that was too large occurred, for example an ACPI "
		"ArgN or LocalN were N was out of range."
	},
	{ AE_AML_REGISTER_LIMIT, 	LOG_LEVEL_HIGH,		"AEAMLRegisterLimit",	"Register limit",
		"An attempt to use a bank value that is beyong the capacity of a register occurred."
	},
	{ AE_AML_NO_WHILE, 		LOG_LEVEL_CRITICAL,	"AEAMLNoWhile",		"No while",
		"A Break or Continue op-code was reached without a matching While op-code."
	},
	{ AE_AML_ALIGNMENT, 		LOG_LEVEL_CRITICAL,	"AEAMLAlignment",	"Misaligmnent",
		"A non-aligned memory transfer was attempted on a machine that does not "
		"support it."
	},
	{ AE_AML_NO_RESOURCE_END_TAG, 	LOG_LEVEL_HIGH,		"AEAMLNoResEndTag",	"No resource end tag",
		"An ACPI resource list had a missing End Tag."
	},
	{ AE_AML_BAD_RESOURCE_VALUE, 	LOG_LEVEL_HIGH,		"AEAMLBadResValue",	"Bad resource value",
		"An ACPI resource element had an invalid value."
	},
	{ AE_AML_CIRCULAR_REFERENCE,	LOG_LEVEL_CRITICAL,	"AEAMLCircularRef",	"Circular reference",
		"Two references refer to each other, which is not allowed."
	},
	{ AE_AML_BAD_RESOURCE_LENGTH,	LOG_LEVEL_HIGH,		"AEAMLBadResLength",	"Bad resource length",
		"The length of an ACPI Resource Descriptor was incorrect."
	},
	{ AE_AML_ILLEGAL_ADDRESS, 	LOG_LEVEL_CRITICAL,	"AEAMLIllegalAddr",	"Illegal address",
		"An memory, PCI configuration or I/O address was encountered with an illegal address."
	},
	/* { AE_AML_INFINITE_LOOP, 	LOG_LEVEL_HIGH,		"AEAMLInfiniteLoop",	"Infinite loop", NULL }, */
	{ 0,				0,			NULL,			NULL , 		NULL}
};

static fwts_list *fwts_object_names;
static bool fwts_acpi_initialized = false;

/*
 *  fwts_acpi_init()
 *	Initialise ACPIA engine and collect method namespace
 */
int fwts_acpi_init(fwts_framework *fw)
{
	if (fwts_acpica_init(fw) != FWTS_OK)
		return FWTS_ERROR;

	/* Gather all object names */
	fwts_object_names = fwts_acpica_get_object_names(0);
	fwts_acpi_initialized = true;

	return FWTS_OK;
}

/*
 *  fwts_acpi_deinit()
 *	Close ACPIA engine and free method namespace
 */
int fwts_acpi_deinit(fwts_framework *fw)
{
	int ret = FWTS_ERROR;

	FWTS_UNUSED(fw);

	if (fwts_acpi_initialized) {
		fwts_list_free(fwts_object_names, free);
		fwts_object_names = NULL;
		ret = fwts_acpica_deinit();

		fwts_acpi_initialized = false;
	}

	return ret;
}

/*
 *  fwts_acpi_object_get_names()
 *	return list of object names
 */
fwts_list *fwts_acpi_object_get_names(void)
{
	return fwts_object_names;
}

/*
 *  fwts_acpi_object_exists()
 *	return first matching name
 */
char *fwts_acpi_object_exists(const char *name)
{
	size_t name_len = strlen(name);
	fwts_list_link	*item;

	fwts_list_foreach(item, fwts_object_names) {
		char *method_name = fwts_list_data(char*, item);
		size_t len = strlen(method_name);

		if (strncmp(name, method_name + len - name_len, name_len) == 0)
			return method_name;
	}
	return NULL;
}


/*
 *   fwts_acpi_object_dump_recursive()
 *	dump out an object, minimal form
 */
static void fwts_acpi_object_dump_recursive(
	fwts_framework *fw,
	const ACPI_OBJECT *obj,
	const int depth,
	const int index)
{
	uint32_t i;
	char index_buf[5];
	ACPI_BUFFER buffer;
	ACPI_STATUS status;
	char full_name[128];

	buffer.Length = sizeof(full_name);
	buffer.Pointer = full_name;

	if (index > -1)
		snprintf(index_buf, sizeof(index_buf), "%2.2d: ", index);
	else
		index_buf[0] = '\0';

	switch (obj->Type) {
	case ACPI_TYPE_INTEGER:
		fwts_log_info_verbatim(fw, "%*s%sINTEGER: 0x%8.8llx", depth * 2, "",
			index_buf, (unsigned long long)obj->Integer.Value);
		break;
	case ACPI_TYPE_STRING:
		fwts_log_info_verbatim(fw, "%*s%sSTRING:  %s", depth * 2, "",
			index_buf, obj->String.Pointer);
		break;
	case ACPI_TYPE_BUFFER:
		fwts_log_info_verbatim(fw, "%*s%sBUFFER:  (%d bytes)", depth * 2, "",
			index_buf, obj->Buffer.Length);
		break;
	case ACPI_TYPE_PACKAGE:
		fwts_log_info_verbatim(fw, "%*s%sPackage has %d elements:",depth * 2, "",
			index_buf, obj->Package.Count);
		for (i = 0; i < obj->Package.Count; i++) {
			ACPI_OBJECT *element = &obj->Package.Elements[i];
			fwts_acpi_object_dump_recursive(fw, element, depth + 1, i);
		}
		break;
	case ACPI_TYPE_LOCAL_REFERENCE:
		status = AcpiGetName(obj->Reference.Handle, ACPI_FULL_PATHNAME, &buffer);
		if (ACPI_SUCCESS(status))
			fwts_log_info_verbatim(fw, "%*s%sReference:  %s", depth * 2, "",
				index_buf, full_name);
		break;
	default:
		fwts_log_info_verbatim(fw, "%*s%sUnknown type 0x%2.2x\n", depth * 2, "",
			index_buf, obj->Type);
		break;
	}
}

/*
 *   fwts_method_object_dump()
 *	dump out an object, minimal form
 */
void fwts_acpi_object_dump(fwts_framework *fw, const ACPI_OBJECT *obj)
{
	fwts_acpi_object_dump_recursive(fw, obj, 1, -1);
}

/*
 *  fwts_acpi_object_evaluate_report_error()
 *	report any errors found during object evaluation
 */
void fwts_acpi_object_evaluate_report_error(
	fwts_framework *fw,
	const char *name,
	const ACPI_STATUS status)
{
	int i;

	/* Generic cases */
	for (i = 0; errors[i].error_type; i++) {
		if (status == errors[i].status) {
			fwts_failed(fw, errors[i].level, errors[i].error_type,
				"Detected error '%s' when evaluating '%s'.",
				errors[i].error_text, name);
			if (errors[i].advice != NULL)
				fwts_advice(fw, "%s", errors[i].advice);
			return;
		}
	}

	/* Special cases */
	switch (status) {
	case AE_OK:
		break;
	case AE_AML_LOOP_TIMEOUT:
		fwts_warning(fw, "Detected an infinite loop when evaluating method '%s'. ", name);
		fwts_advice(fw, "This may occur because we are emulating the execution "
				"in this test environment and cannot handshake with "
				"the embedded controller or jump to the BIOS via SMIs. "
				"However, the fact that AML code spins forever means that "
				"lockup conditions are not being checked for in the AML bytecode.");
		break;
	/* Unknown?! */
	default:
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "AMLFailedToEvaluate", "Failed to evaluate '%s', got error code %d.", name, status);
		break;
	}
}

/*
 *  fwts_acpi_object_evaluate()
 *	evaluate object, return error status (handle this with
 *	fwts_method_evaluate_report_error()).
 * 	This returns buf which auto allocates any return values
 *	which need to be freed post-evalutation using free().
 */
ACPI_STATUS fwts_acpi_object_evaluate(fwts_framework *fw,
	char *name,
        ACPI_OBJECT_LIST *arg_list,
	ACPI_BUFFER	 *buf)
{
	FWTS_UNUSED(fw);

	buf->Length  = ACPI_ALLOCATE_BUFFER;
	buf->Pointer = NULL;

	return AcpiEvaluateObject(NULL, name, arg_list, buf);
}

int fwts_method_check_type__(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT_TYPE type,
	char *type_name)
{
	ACPI_OBJECT *obj;

	if ((buf == NULL) || (buf->Pointer == NULL)) {
		fwts_method_failed_null_object(fw, name, type_name);
		return FWTS_ERROR;
	}

	obj = buf->Pointer;

	if (!fwts_method_type_matches(obj->Type, type)) {
		fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodReturnBadType",
			"Method %s did not return %s.", name, type_name);
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  Common types that can be returned. This is not a complete
 *  list but it does cover the types we expect to return from
 *  an ACPI evaluation.
 */
const char *fwts_method_type_name(const ACPI_OBJECT_TYPE type)
{
	switch (type) {
	case ACPI_TYPE_INTEGER:
		return "integer";
	case ACPI_TYPE_STRING:
		return "string";
	case ACPI_TYPE_BUFFER:
		return "buffer";
	case ACPI_TYPE_PACKAGE:
		return "package";
	case ACPI_TYPE_BUFFER_FIELD:
		return "buffer_field";
	case ACPI_TYPE_LOCAL_REFERENCE:
		return "reference";
	case ACPI_TYPE_INTBUF:
		return "integer or buffer";
	default:
		return "unknown";
	}
}

/*
 *  method_passed_sane()
 *	helper function to report often used passed messages
 */
void fwts_method_passed_sane(
	fwts_framework *fw,
	const char *name,
	const char *type)
{
	fwts_passed(fw, "%s correctly returned a sane looking %s.", name, type);
}

/*
 *  method_passed_sane_uint64()
 *	helper function to report often used passed uint64 values
 */
void fwts_method_passed_sane_uint64(
	fwts_framework *fw,
	const char *name,
	const uint64_t value)
{
	fwts_passed(fw, "%s correctly returned sane looking "
		"value 0x%8.8" PRIx64 ".", name, value);
}

/*
 *  fwts_method_failed_null_object()
 *	helper function to report often used failed NULL object return
 */
void fwts_method_failed_null_object(
	fwts_framework *fw,
	const char *name,
	const char *type)
{
	fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodReturnNullObj",
		"%s returned a NULL object, and did not "
		"return %s.", name, type);
}

bool fwts_method_type_matches(ACPI_OBJECT_TYPE t1, ACPI_OBJECT_TYPE t2)
{
	if (t1 == ACPI_TYPE_INTBUF &&
	    (t2 == ACPI_TYPE_INTEGER || t2 == ACPI_TYPE_BUFFER))
		return true;

	if (t2 == ACPI_TYPE_INTBUF &&
	    (t1 == ACPI_TYPE_INTEGER || t1 == ACPI_TYPE_BUFFER))
		return true;

	return t1 == t2;
}

/*
 *  method_package_count_min()
 *	check that an ACPI package has at least 'min' elements
 */
int fwts_method_package_count_min(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const ACPI_OBJECT *obj,
	const uint32_t min)
{
	if (obj->Package.Count < min) {
		char tmp[128];

		snprintf(tmp, sizeof(tmp), "Method%sElementCount", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return package of at least %" PRIu32
			" element%s, got %" PRIu32 " element%s instead.",
			name, min, min == 1 ? "" : "s",
			obj->Package.Count, obj->Package.Count == 1 ? "" : "s");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

/*
 *  method_package_count_equal()
 *	check that an ACPI package has exactly 'count' elements
 */
int fwts_method_package_count_equal(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const ACPI_OBJECT *obj,
	const uint32_t count)
{
	if (obj->Package.Count != count) {
		char tmp[128];

		snprintf(tmp, sizeof(tmp), "Method%sElementCount", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return package of %" PRIu32
			" element%s, got %" PRIu32 " element%s instead.",
			name, count, count == 1 ? "" : "s",
			obj->Package.Count, obj->Package.Count == 1 ? "" : "s");
		return FWTS_ERROR;
	}
	return FWTS_OK;
}

int fwts_method_package_elements_all_type(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const ACPI_OBJECT *obj,
	const ACPI_OBJECT_TYPE type)
{
	uint32_t i;
	bool failed = false;
	char tmp[128];

	for (i = 0; i < obj->Package.Count; i++) {
		if (!fwts_method_type_matches(obj->Package.Elements[i].Type, type)) {
			snprintf(tmp, sizeof(tmp), "Method%sElementType", objname);
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
				"%s package element %" PRIu32 " was not the expected "
				"type '%s', was instead type '%s'.",
				name, i,
				fwts_method_type_name(type),
				fwts_method_type_name(obj->Package.Elements[i].Type));
			failed = true;
		}
	}

	return failed ? FWTS_ERROR: FWTS_OK;
}

/*
 *  fwts_method_package_elements_type()
 *	sanity check fields in a package that all have
 *	the same type
 */
int fwts_method_package_elements_type(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const ACPI_OBJECT *obj,
	const fwts_package_element *info,
	const uint32_t count)
{
	uint32_t i;
	bool failed = false;
	char tmp[128];

	if (obj->Package.Count != count)
		return FWTS_ERROR;

	for (i = 0; i < obj->Package.Count; i++) {
		if (!fwts_method_type_matches(obj->Package.Elements[i].Type, info[i].type)) {
			snprintf(tmp, sizeof(tmp), "Method%sElementType", objname);
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
				"%s package element %" PRIu32 " (%s) was not the expected "
				"type '%s', was instead type '%s'.",
				name, i, info[i].name,
				fwts_method_type_name(info[i].type),
				fwts_method_type_name(obj->Package.Elements[i].Type));
			failed = true;
		}
	}

	return failed ? FWTS_ERROR: FWTS_OK;
}

/*
 *  fwts_method_test_integer_return
 *	check if an integer object was returned
 */
void fwts_method_test_integer_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned an integer.", name);
}

/*
 *  fwts_method_test_string_return
 *	check if an string object was returned
 */
void fwts_method_test_string_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_STRING) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned a string.", name);
}

/*
 *  fwts_method_test_reference_return
 *	check if a reference object was returned
 */
void fwts_method_test_reference_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(obj);
	FWTS_UNUSED(private);

	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_LOCAL_REFERENCE) == FWTS_OK)
		fwts_passed(fw, "%s correctly returned a reference.", name);
}

/*
 *  fwts_method_test_NULL_return
 *	check if no object was retuned
 */
void fwts_method_test_NULL_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	FWTS_UNUSED(private);

	/*
	 *  In ACPICA SLACK mode null returns can be actually
	 *  forced to return ACPI integers. Blame an errata
	 *  and Windows compatibility for this mess.
	 */
	if (fw->acpica_mode & FWTS_ACPICA_MODE_SLACK) {
		if ((buf != NULL) && (buf->Pointer != NULL)) {
			ACPI_OBJECT *objtmp = buf->Pointer;
			if (fwts_method_type_matches(objtmp->Type, ACPI_TYPE_INTEGER)) {
				fwts_passed(fw, "%s returned an ACPI_TYPE_INTEGER as expected in slack mode.",
					name);
				return;
			}
		}
	}

	if (buf && buf->Length && buf->Pointer) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "MethodShouldReturnNothing", "%s returned values, but was expected to return nothing.", name);
		fwts_log_info(fw, "Object returned:");
		fwts_acpi_object_dump(fw, obj);
		fwts_advice(fw,
			"This probably won't cause any errors, but it should "
			"be fixed as the AML code is not conforming to the "
			"expected behaviour as described in the ACPI "
			"specification.");
	} else
		fwts_passed(fw, "%s returned no values as expected.", name);
}

/*
 *  fwts_method_test_passed_failed_return
 *	check if 0 or 1 (false/true) integer is returned
 */
void fwts_method_test_passed_failed_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	char *method = (char *)private;
	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) == FWTS_OK) {
		uint32_t val = (uint32_t)obj->Integer.Value;
		if ((val == 0) || (val == 1))
			fwts_method_passed_sane_uint64(fw, name, obj->Integer.Value);
		else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"MethodReturnZeroOrOne",
				"%s returned 0x%8.8" PRIx32 ", should return 1 "
				"(success) or 0 (failed).", method, val);
			fwts_advice(fw,
				"Method %s should be returning the correct "
				"1/0 success/failed return values. "
				"Unexpected behaviour may occur becauses of "
				"this error, the AML code does not conform to "
				"the ACPI specification and should be fixed.",
				method);
		}
	}
}

/*
 *  fwts_method_test_polling_return
 *	check if a returned polling time is valid
 */
void fwts_method_test_polling_return(
	fwts_framework *fw,
	char *name,
	ACPI_BUFFER *buf,
	ACPI_OBJECT *obj,
	void *private)
{
	if (fwts_method_check_type(fw, name, buf, ACPI_TYPE_INTEGER) == FWTS_OK) {
		char *method = (char *)private;
		if (obj->Integer.Value < 36000) {
			fwts_passed(fw,
				"%s correctly returned sane looking value "
				"%f seconds", method,
				(float)obj->Integer.Value / 10.0);
		} else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL,
				"MethodPollTimeTooLong",
				"%s returned a value %f seconds > (1 hour) "
				"which is probably incorrect.",
				method, (float)obj->Integer.Value / 10.0);
			fwts_advice(fw,
				"The method is returning a polling interval "
				"which is very long and hence most probably "
				"incorrect.");
		}
	}
}

void fwts_evaluate_found_method(
	fwts_framework *fw,
	ACPI_HANDLE *parent,
	char *name,
	fwts_method_return check_func,
	void *private,
	ACPI_OBJECT_LIST *arg_list)
{
	ACPI_BUFFER	buf;
	ACPI_STATUS	status;
	int sem_acquired;
	int sem_released;

	fwts_acpica_sem_count_clear();

	buf.Length  = ACPI_ALLOCATE_BUFFER;
	buf.Pointer = NULL;
	status = AcpiEvaluateObject(*parent, name, arg_list, &buf);

	if (ACPI_SUCCESS(status) && check_func != NULL) {
		ACPI_OBJECT *obj = buf.Pointer;
		check_func(fw, name, &buf, obj, private);
	}
	free(buf.Pointer);

	fwts_acpica_sem_count_get(&sem_acquired, &sem_released);
	if (sem_acquired != sem_released) {
		fwts_failed(fw, LOG_LEVEL_MEDIUM, "AMLLocksAcquired",
			"%s left %d locks in an acquired state.",
			name, sem_acquired - sem_released);
		fwts_advice(fw,
			"Locks left in an acquired state generally indicates "
			"that the AML code is not releasing a lock. This can "
			"sometimes occur when a method hits an error "
			"condition and exits prematurely without releasing an "
			"acquired lock. It may be occurring in the method "
			"being tested or other methods used while evaluating "
			"the method.");
	}
}

int fwts_evaluate_method(fwts_framework *fw,
	uint32_t test_type,  /* Manditory or optional */
	ACPI_HANDLE *parent,
	char *name,
	ACPI_OBJECT *args,
	uint32_t num_args,
	fwts_method_return check_func,
	void *private)
{
	ACPI_OBJECT_LIST	arg_list;
	ACPI_HANDLE	method;
	ACPI_STATUS	status;

	status = AcpiGetHandle (*parent, name, &method);
	if (ACPI_SUCCESS(status)) {
		arg_list.Count   = num_args;
		arg_list.Pointer = args;
		fwts_evaluate_found_method(fw, parent, name, check_func, private, &arg_list);
	}

	if (status == AE_NOT_FOUND && !(test_type & METHOD_SILENT)) {
		if (test_type & METHOD_MANDATORY) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, "MethodNotExist", "Object %s did not exist.", name);
			return FWTS_ERROR;
		}

		if (test_type & METHOD_OPTIONAL) {
			fwts_skipped(fw, "Skipping test for non-existent object %s.", name);
			return FWTS_SKIP;
		}
	}

	return FWTS_OK;
}

bool fwts_method_valid_HID_string(char *str)
{
	if (strlen(str) == 7) {
		/* PNP ID, must be 3 capitals followed by 4 hex */
		if (!isupper(str[0]) ||
		    !isupper(str[1]) ||
		    !isupper(str[2])) return false;
		if (!isxdigit(str[3]) ||
		    !isxdigit(str[4]) ||
		    !isxdigit(str[5]) ||
		    !isxdigit(str[6])) return false;
		return true;
	}

	if (strlen(str) == 8) {
		/* ACPI ID, must be 4 capitals or digits followed by 4 hex */
		if ((!isupper(str[0]) && !isdigit(str[0])) ||
		    (!isupper(str[1]) && !isdigit(str[1])) ||
		    (!isupper(str[2]) && !isdigit(str[2])) ||
		    (!isupper(str[3]) && !isdigit(str[3]))) return false;
		if (!isxdigit(str[4]) ||
		    !isxdigit(str[5]) ||
		    !isxdigit(str[6]) ||
		    !isxdigit(str[7])) return false;
		return true;
	}

	return false;
}

bool fwts_method_valid_EISA_ID(uint32_t id, char *buf, size_t buf_len)
{
	snprintf(buf, buf_len, "%c%c%c%02" PRIX32 "%02" PRIX32,
		0x40 + ((id >> 2) & 0x1f),
		0x40 + ((id & 0x3) << 3) + ((id >> 13) & 0x7),
		0x40 + ((id >> 8) & 0x1f),
		(id >> 16) & 0xff, (id >> 24) & 0xff);

	/* 3 chars in EISA ID must be upper case */
	if (!isupper(buf[0]) ||
	    !isupper(buf[1]) ||
	    !isupper(buf[2])) return false;

	/* Last 4 digits are always going to be hex, so pass */
	return true;
}

/*
 *  Section 6.2 Device Configurations Objects
 */
static void method_test_CRS_size(
	fwts_framework *fw,
	const char *name,		/* full _CRS or _PRS path name */
	const char *objname,		/* name of _CRS or _PRS object */
	const char *tag,		/* error log tag */
	const size_t crs_length,	/* size of _CRS buffer */
	const size_t hdr_length,	/* size of _CRS header */
	const size_t data_length,	/* length of _CRS data w/o header */
	const size_t min,		/* minimum allowed _CRS data size */
	const size_t max,		/* maximum allowed _CRS data size */
	bool *passed)			/* pass/fail flag */
{
	if (crs_length < data_length + hdr_length) {
		fwts_failed(fw, LOG_LEVEL_HIGH, tag,
			"%s Resource size is %zd bytes long but "
			"the size stated in the %s buffer header  "
			"is %zd and hence is longer. The resource "
			"buffer is too short.",
			name, crs_length, objname, data_length);
		*passed = false;
		return;
	}

	if ((data_length < min) || (data_length > max)) {
		if (min != max) {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tag,
				"%s Resource data size was %zd bytes long, "
				"expected it to be between %zd and %zd bytes",
				name, data_length, min, max);
			*passed = false;
		} else {
			fwts_failed(fw, LOG_LEVEL_CRITICAL, tag,
				"%s Resource data size was %zd bytes long, "
				"expected it to be %zd bytes",
				name, data_length, min);
			*passed = false;
		}
	}
}

static void method_test_CRS_small_size(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const size_t crs_length,
	const size_t min,
	const size_t max,
	bool *passed)
{
	size_t data_length = data[0] & 7;
	char tmp[128];

	snprintf(tmp, sizeof(tmp), "Method%sSmallResourceSize", objname);

	method_test_CRS_size(fw, name, objname, tmp,
		crs_length, 1, data_length, min, max, passed);
}

/*
 *  CRS small resource checks, simple checking
 */
void fwts_method_test_CRS_small_resource_items(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const size_t length,
	bool *passed,
	const char **tag)
{
	uint8_t tag_item = (data[0] >> 3) & 0xf;
	char tmp[128];

	static const char *types[] = {
		"Reserved",
		"Reserved",
		"Reserved",
		"Reserved",
		"IRQ Descriptor",
		"DMA Descriptor",
		"Start Dependent Functions Descriptor",
		"End Dependent Functions Descriptor",
		"I/O Port Descriptor",
		"Fixed Location I/O Port Descriptor",
		"Fixed DMA Descriptor",
		"Reserved",
		"Reserved",
		"Reserved",
		"Vendor Defined Descriptor",
		"End Tag Descriptor"
	};

	switch (tag_item) {
	case 0x4: /* 6.4.2.1 IRQ Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 2, 3, passed);
		break;
	case 0x5: /* 6.4.2.2 DMA Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 2, 2, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if ((data[2] & 3) == 3) {
			snprintf(tmp, sizeof(tmp), "Method%sDmaDescriptor", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s DMA transfer type preference is 0x%" PRIx8
				" which is reserved and invalid. See "
				"Section 6.4.2.2 of the ACPI specification.",
				name, data[2] & 3);
			*passed = false;
		}
		break;
	case 0x6: /* 6.4.2.3 Start Dependent Functions Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 0, 1, passed);
		break;
	case 0x7: /* 6.4.2.4 End Dependent Functions Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 0, 0, passed);
		break;
	case 0x8: /* 6.4.2.5 I/O Port Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 7, 7, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if (data[1] & 0xfe) {
			snprintf(tmp, sizeof(tmp), "Method%sIoPortInfoReservedNonZero", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s I/O Port Descriptor Information field "
				"has reserved bits that are non-zero, got "
				"0x%" PRIx8 " and expected 0 or 1 for this "
				"field. ", name, data[1]);
			*passed = false;
		}
		if (((data[1] & 1) == 0) && (data[3] > 3)) {
			snprintf(tmp, sizeof(tmp), "Method%sIoPortInfoMinBase10BitAddr", objname);
			fwts_failed(fw, LOG_LEVEL_LOW, tmp,
				"%s I/O Port Descriptor range minimum "
				"base address is more than 10 bits however "
				"the Information field indicates that only "
				"a 10 bit address is being used.", name);
			*passed = false;
		}
		if (((data[1] & 1) == 0) && (data[5] > 3)) {
			snprintf(tmp, sizeof(tmp), "Method%sIoPortInfoMaxBase10BitAddr", objname);
			fwts_failed(fw, LOG_LEVEL_LOW, tmp,
				"%s I/O Port Descriptor range maximum "
				"base address is more than 10 bits however "
				"the Information field indicates that only "
				"a 10 bit address is being used.", name);
			*passed = false;
		}
		break;
	case 0x9: /* 6.4.2.6 Fixed Location I/O Port Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 3, 3, passed);
		break;
	case 0xa: /* 6.4.2.7 Fixed DMA Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 5, 5, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if (data[5] > 5) {
			snprintf(tmp, sizeof(tmp), "Method%sFixedDmaTransferWidth", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s DMA transfer width is 0x%" PRIx8
				" which is reserved and invalid. See "
				"Section 6.4.2.7 of the ACPI specification.",
				name, data[5]);
			*passed = false;
		}
		break;
	case 0xe: /* 6.4.2.8 Vendor-Defined Descriptor */
		method_test_CRS_small_size(fw, name, objname, data, length, 1, 7, passed);
		break;
	case 0xf: /* 6.4.2.9 End Tag */
		method_test_CRS_small_size(fw, name, objname, data, length, 1, 1, passed);
		break;
	default:
		snprintf(tmp, sizeof(tmp), "Method%sUnkownSmallResourceItem", objname);
		fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
			"%s tag bits 6:3 is an undefined "
			"small tag item name, value 0x%" PRIx8 ".",
			name, tag_item);
		fwts_advice(fw,
			"A small resource data type tag (byte 0, "
			"bits 6:3 of the %s buffer) contains "
			"an undefined small tag item 'name'. "
			"The %s buffer is therefore undefined "
			"and can't be used.  See section "
			"'6.4.2 Small Resource Data Type' of the ACPI "
			"specification, and also table 6-161.",
			objname, objname);
		*passed = false;
		break;
	}

	*tag = types[tag_item];
}



void fwts_method_test_CRS_large_size(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const size_t crs_length,
	const size_t min,
	const size_t max,
	bool *passed)
{
	size_t data_length;
	char tmp[128];

	/* Small _CRS resources have a 3 byte header */
	if (crs_length < 3) {
		snprintf(tmp, sizeof(tmp), "Method%sBufferTooSmall", objname);
		fwts_failed(fw, LOG_LEVEL_CRITICAL, tmp,
			"%s should return a buffer of at least three bytes in length.", name);
		*passed = false;
		return;
	}

	data_length = (size_t)data[1] + ((size_t)data[2] << 8);

	snprintf(tmp, sizeof(tmp), "Method%sLargeResourceSize",objname);
	method_test_CRS_size(fw, name, objname, tmp,
		crs_length, 3, data_length, min, max, passed);
}

/*
 * Some CRS value fetching helper functions.  We handle all the
 * addresses and lengths in 64 bits to make life easier
 */
static uint64_t method_CRS_val64(const uint8_t *data)
{
	uint64_t val =
		((uint64_t)data[7] << 56) | ((uint64_t)data[6] << 48) |
		((uint64_t)data[5] << 40) | ((uint64_t)data[4] << 32) |
		((uint64_t)data[3] << 24) | ((uint64_t)data[2] << 16) |
		((uint64_t)data[1] << 8)  | (uint64_t)data[0];

	return val;
}

static uint64_t method_CRS_val32(const uint8_t *data)
{
	uint64_t val =
		((uint64_t)data[3] << 24) | ((uint64_t)data[2] << 16) |
		((uint64_t)data[1] << 8)  | (uint64_t)data[0];

	return val;
}

static uint64_t method_CRS_val24(const uint8_t *data)
{
	/* 24 bit values assume lower 8 bits are zero */
	uint64_t val =
		((uint64_t)data[1] << 16) | ((uint64_t)data[0] << 8);

	return val;
}

static uint64_t method_CRS_val16(const uint8_t *data)
{
	uint64_t val =
		((uint64_t)data[1] << 8) | (uint64_t)data[0];

	return val;
}

/*
 *  Sanity check addresses according to table 6-179 of ACPI spec
 */
static void method_test_CRS_mif_maf(
	fwts_framework *fw,
	const char *name,		/* Full _CRS or _PRS path name */
	const char *objname,		/* _CRS or _PRS name */
	const uint8_t flag,		/* _MIF _MAF flag field */
	const uint64_t min,		/* Min address */
	const uint64_t max,		/* Max address */
	const uint64_t len,		/* Range length */
	const uint64_t granularity,	/* Address granularity */
	const char *tag,		/* failed error tag */
	const char *type,		/* Resource type */
	bool *passed)
{
	char tmp[128];
	uint8_t mif = (flag >> 2) & 1;
	uint8_t maf = (flag >> 3) & 1;

	static char *mif_maf_advice =
		"See section '6.4.3.5 Address Space Resource Descriptors' "
		"table 6-179 of the ACPI specification for more details "
		"about how the _MIF, _MAF and memory range and granularity "
		"rules apply. Typically the kernel does not care about these "
		"being correct, so this is a minor issue.";

	/* Table 6-179 Valid combination of Address Space Descriptors fields */
	if (len == 0) {
		if ((mif == 1) && (maf == 1)) {
			snprintf(tmp, sizeof(tmp), "Method%s%sMifMafBothOne", objname, tag);
			if (fw->flags & FWTS_FLAG_TEST_SBBR)
				fwts_warning(fw, tmp,
					"%s %s _MIF and _MAF flags are both "
					"set to one which is invalid when "
					"the length field is 0.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s _MIF and _MAF flags are both "
					"set to one which is invalid when "
					"the length field is 0.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
		if ((mif == 1) && (min % (granularity + 1) != 0)) {
			snprintf(tmp, sizeof(tmp), "Method%s%sMinNotMultipleOfGran", objname, tag);
			if (fw->flags & FWTS_FLAG_TEST_SBBR)
				fwts_warning(fw, tmp,
					"%s %s _MIN address is not a multiple "
					"of the granularity when _MIF is 1.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s _MIN address is not a multiple "
					"of the granularity when _MIF is 1.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
		if ((maf == 1) && (max % (granularity - 1) != 0)) {
			snprintf(tmp, sizeof(tmp), "Method%s%sMaxNotMultipleOfGran", objname, tag);
			if (fw->flags & FWTS_FLAG_TEST_SBBR)
				fwts_warning(fw, tmp,
					"%s %s _MAX address is not a multiple "
					"of the granularity when _MAF is 1.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s _MAX address is not a multiple "
					"of the granularity when _MAF is 1.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
	} else {
		if ((mif == 0) && (maf == 0) &&
		    (len % (granularity + 1) != 0)) {
			snprintf(tmp, sizeof(tmp), "Method%s%sLenNotMultipleOfGran", objname, tag);
			if (fw->flags & FWTS_FLAG_TEST_SBBR)
				fwts_warning(fw, tmp,
					"%s %s length is not a multiple "
					"of the granularity when _MIF "
					"and _MIF are 0.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s length is not a multiple "
					"of the granularity when _MIF "
					"and _MIF are 0.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
		if (((mif == 0) && (maf == 1)) || ((mif == 1) && (maf == 0))) {
			snprintf(tmp, sizeof(tmp), "Method%s%sMifMafInvalid", objname, tag);
			if (fw->flags & FWTS_FLAG_TEST_SBBR)
				fwts_warning(fw, tmp,
					"%s %s _MIF and _MAF flags are either "
					"0 and 1 or 1 and 0 which is invalid when "
					"the length field is non-zero.",
					name, type);
			else
				fwts_failed(fw, LOG_LEVEL_MEDIUM,
					tmp,
					"%s %s _MIF and _MAF flags are either "
					"0 and 1 or 1 and 0 which is invalid when "
					"the length field is non-zero.",
					name, type);
			fwts_advice(fw, "%s", mif_maf_advice);
			*passed = false;
		}
		if ((mif == 1) && (maf == 1)) {
			if (granularity != 0) {
				snprintf(tmp, sizeof(tmp), "Method%s%sGranularityNotZero", objname, tag);
				if (fw->flags & FWTS_FLAG_TEST_SBBR)
					fwts_warning(fw, tmp,
						"%s %s granularity 0x%" PRIx64
						" is not zero as expected when "
						"_MIF and _MAF are both 1.",
						name, type, granularity);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s granularity 0x%" PRIx64
						" is not zero as expected when "
						"_MIF and _MAF are both 1.",
						name, type, granularity);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
			if (min > max) {
				snprintf(tmp, sizeof(tmp), "Method%s%sMaxLessThanMin", objname, tag);
				if (fw->flags & FWTS_FLAG_TEST_SBBR)
					fwts_warning(fw, tmp,
						"%s %s minimum address range 0x%" PRIx64
						" is greater than the maximum address "
						"range 0x%" PRIx64 ".",
						name, type, min, max);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s minimum address range 0x%" PRIx64
						" is greater than the maximum address "
						"range 0x%" PRIx64 ".",
						name, type, min, max);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
			if (max - min + 1 != len) {
				snprintf(tmp, sizeof(tmp), "Method%s%sLengthInvalid", objname, tag);
				if (fw->flags & FWTS_FLAG_TEST_SBBR)
					fwts_warning(fw, tmp,
						"%s %s length 0x%" PRIx64
						" does not match the difference between "
						"the minimum and maximum address ranges "
						"0x%" PRIx64 "-0x%" PRIx64 ".",
						name, type, len, min, max);
				else
					fwts_failed(fw, LOG_LEVEL_MEDIUM,
						tmp,
						"%s %s length 0x%" PRIx64
						" does not match the difference between "
						"the minimum and maximum address ranges "
						"0x%" PRIx64 "-0x%" PRIx64 ".",
						name, type, len, min, max);
				fwts_advice(fw, "%s", mif_maf_advice);
				*passed = false;
			}
		}
	}
}

/*
 *  CRS large resource checks, simple checking
 */
void fwts_method_test_CRS_large_resource_items(
	fwts_framework *fw,
	const char *name,
	const char *objname,
	const uint8_t *data,
	const uint64_t length,
	bool *passed,
	const char **tag)
{
	uint64_t min, max, len, gra;
	uint8_t tag_item = data[0] & 0x7f;
	char tmp[128];

	static const char *types[] = {
		"Reserved",
		"24-bit Memory Range Descriptor",
		"Generic Register Descriptor",
		"Reserved",
		"Vendor Defined Descriptor",
		"32-bit Memory Range Descriptor",
		"32-bit Fixed Location Memory Range Descriptor",
		"DWORD Address Space Descriptor",
		"WORD Address Space Descriptor",
		"Extended IRQ Descriptor",
		"QWORD Address Space Descriptor",
		"Extended Addresss Space Descriptor",
		"GPIO Connection Descriptor",
		"Reserved",
		"Generic Serial Bus Connection Descriptor",
		"Pin Configuration Descriptor",
		"Pin Group Descriptor",
		"Pin Group Function Descriptor",
		"Pin Group Configuration Descriptor",
		"Reserved",
	};

	switch (tag_item) {
	case 0x1: /* 6.4.3.1 24-Bit Memory Range Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 9, 9, passed);
		if (!*passed)	/* Too short, abort */
			break;
		min = method_CRS_val24(&data[4]);
		max = method_CRS_val24(&data[6]);
		len = method_CRS_val16(&data[10]);
		if (max < min) {
			snprintf(tmp, sizeof(tmp), "Method%s24BitMemRangeMaxLessThanMin", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 24-Bit Memory Range Descriptor minimum "
				"address range 0x%" PRIx64 " is greater than "
				"the maximum address range 0x%" PRIx64 ".",
				name, min, max);
			*passed = false;
		}
		if (len > max + 1 - min) {
			snprintf(tmp, sizeof(tmp), "Method%s24BitMemRangeLengthTooLarge", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 24-Bit Memory Range Descriptor length "
				"0x%" PRIx64 " is greater than size between the "
				"the minimum and maximum address ranges "
				"0x%" PRIx64 "-0x%" PRIx64 ".",
				name, len, min, max);
			*passed = false;
		}
		break;
	case 0x2: /* 6.4.3.7 Generic Register Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 12, 12, passed);
		if (!*passed)
			break;
		switch (data[3]) {
		case 0x00 ... 0x04:
		case 0x0a:
		case 0x7f:
			/* Valid values */
			break;
		default:
			snprintf(tmp, sizeof(tmp), "Method%sGenericRegAddrSpaceIdInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s Generic Register Descriptor has an invalid "
				"Address Space ID 0x%" PRIx8 ".",
				name, data[3]);
			*passed = false;
		}
		if (data[6] > 4) {
			snprintf(tmp, sizeof(tmp), "Method%sGenericRegAddrSizeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s Generic Register Descriptor has an invalid "
				"Address Access Size 0x%" PRIx8 ".",
				name, data[6]);
			*passed = false;
		}
		break;
	case 0x4: /* 6.4.3.2 Vendor-Defined Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 0, 65535, passed);
		break;
	case 0x5: /* 6.4.3.3 32-Bit Memory Range Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 17, 17, passed);
		if (!*passed)
			break;
		min = method_CRS_val32(&data[4]);
		max = method_CRS_val32(&data[8]);
		len = method_CRS_val32(&data[16]);
		if (max < min) {
			snprintf(tmp, sizeof(tmp), "Method%s32BitMemRangeMaxLessThanMin", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 32-Bit Memory Range Descriptor minimum "
				"address range 0x%" PRIx64 " is greater than "
				"the maximum address range 0x%" PRIx64 ".",
				name, min, max);
			*passed = false;
		}
		if (len > max + 1 - min) {
			snprintf(tmp, sizeof(tmp), "Method%s32BitMemRangeLengthTooLarge", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s 32-Bit Memory Range Descriptor length "
				"0x%" PRIx64 " is greater than size between the "
				"the minimum and maximum address ranges "
				"0x%" PRIx64 "-0x%" PRIx64 ".",
				name, len, min, max);
			*passed = false;
		}
		break;
	case 0x6: /* 6.4.3.4 32-Bit Fixed Memory Range Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 9, 9, passed);
		/* Not much can be checked for this descriptor */
		break;
	case 0x7: /* 6.4.3.5.2 DWord Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 23, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val32(&data[6]);
		min = method_CRS_val32(&data[10]);
		max = method_CRS_val32(&data[14]);
		len = method_CRS_val32(&data[22]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitDWordAddrSpace",
			types[0x7], passed);
		break;
	case 0x8: /* 6.4.3.5.3 Word Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 13, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val16(&data[6]);
		min = method_CRS_val16(&data[8]);
		max = method_CRS_val16(&data[10]);
		len = method_CRS_val16(&data[14]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitWordAddrSpace",
			types[0x8], passed);
		break;
	case 0x9: /* 6.4.3.6 Extended Interrupt Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 6, 65535, passed);
		/* Not much can be checked for this descriptor */
		break;
	case 0xa: /* 6.4.3.5.1 QWord Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 43, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val64(&data[6]);
		min = method_CRS_val64(&data[14]);
		max = method_CRS_val64(&data[22]);
		len = method_CRS_val64(&data[38]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitQWordAddrSpace",
			types[0xa], passed);
		break;
	case 0xb: /* 6.4.3.5.4 Extended Address Space Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 53, 53, passed);
		if (!*passed)	/* Too short, abort */
			break;
		gra = method_CRS_val64(&data[8]);
		min = method_CRS_val64(&data[16]);
		max = method_CRS_val64(&data[24]);
		len = method_CRS_val64(&data[40]);

		method_test_CRS_mif_maf(fw, name, objname, data[4],
			min, max, len, gra,
			"64BitExtAddrSpace",
			types[0xb], passed);
		break;
	case 0xc: /* 6.4.3.8.1 GPIO Connection Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 22, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		if (data[4] > 2) {
			snprintf(tmp, sizeof(tmp), "Method%sGpioConnTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s GPIO Connection Descriptor has an invalid "
				"Connection Type 0x%" PRIx8 ".",
				name, data[4]);
			*passed = false;
			fwts_advice(fw,
				"The GPIO pin connection type is "
				"not recognised. It should be either "
				"0x00 (interrupt connection) or "
				"0x01 (I/O connection). See section "
				"6.4.3.8.1 of the ACPI specification.");
		}
		if ((data[9] > 0x03) && (data[9] < 0x80)) {
			snprintf(tmp, sizeof(tmp), "Method%sGpioConnTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
				"%s GPIO Connection Descriptor has an invalid "
				"Pin Configuration Type 0x%" PRIx8 ".",
				name, data[9]);
			*passed = false;
			fwts_advice(fw,
				"The GPIO pin configuration type "
				"is not recognised. It should be one of:"
				"0x00 (default), 0x01 (pull-up), "
				"0x02 (pull-down), 0x03 (no-pull), "
				"0x80-0xff (vendor defined). See table "
				"6-189 in section 6.4.3.8.1 of the ACPI "
				"specification.");
		}
		break;
	case 0xd: /* 6.4.3.9 Pin Function Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 17, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;
		fwts_acpi_reserved_bits_check(fw, "_CRS", "Flags[low]", data[4], sizeof(data[4]), 1, 7, passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Flags[high]", data[5], sizeof(data[5]), passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Resource Source Index", data[11], sizeof(data[11]), passed);
		break;
	case 0xe: /* 6.4.3.8.2 Serial Bus Connection Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 11, 65535, passed);
		/* Don't care */
		break;
	case 0xf: /* 6.4.3.10 Pin Configuration Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 19, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits_check(fw, "_CRS", "Flags[low]", data[4], sizeof(data[4]), 2, 7, passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Flags[high]", data[5], sizeof(data[5]), passed);

		if (data[6] > 0xd && data[6] < 0x80) {
			*passed = false;
			snprintf(tmp, sizeof(tmp), "Method%sPinConfigTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s Pin Configuration Descriptor has an invalid "
				"Type 0x%" PRIx8 ".", name, data[6]);
			fwts_advice(fw,
				"The Pin Configuration type is "
				"not recognised. It should be either in "
				"range of 0..0x0D or 0x80..0xFF. See "
				"section 6.4.3.10 of the ACPI spec.");
		}

		fwts_acpi_reserved_zero_check(fw, "_CRS", "Resource Source Index", data[13], sizeof(data[13]), passed);
		break;
	case 0x10: /* 6.4.3.11 Pin Group Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 13, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits_check(fw, "_CRS", "Flags[low]", data[4], sizeof(data[4]), 1, 7, passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Flags[high]", data[5], sizeof(data[5]), passed);

		break;
	case 0x11: /* 6.4.3.12 Pin Group Function Descriptors */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 16, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits_check(fw, "_CRS", "Flags[low]", data[4], sizeof(data[4]), 2, 7, passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Flags[high]", data[5], sizeof(data[5]), passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Resource Source Index", data[8], sizeof(data[8]), passed);
		break;
	case 0x12: /* 6.4.3.13 Pin Group Configuration Descriptor */
		fwts_method_test_CRS_large_size(fw, name, objname, data, length, 19, 65535, passed);
		if (!*passed)	/* Too short, abort */
			break;

		fwts_acpi_reserved_bits_check(fw, "_CRS", "Flags[low]", data[4], sizeof(data[4]), 2, 7, passed);
		fwts_acpi_reserved_zero_check(fw, "_CRS", "Flags[high]", data[5], sizeof(data[5]), passed);

		if (data[6] > 0xd && data[6] < 0x80) {
			*passed = false;
			snprintf(tmp, sizeof(tmp), "Method%sPinConfigTypeInvalid", objname);
			fwts_failed(fw, LOG_LEVEL_MEDIUM, tmp,
				"%s Pin Group Configuration Descriptor has an invalid "
				"Type 0x%" PRIx8 ".", name, data[6]);
			fwts_advice(fw,
				"The Pin Group Configuration type is "
				"not recognised. It should be either in "
				"range of 0..0x0D or 0x80..0xFF. See "
				"section 6.4.3.10 of the ACPI spec.");
		}

		fwts_acpi_reserved_zero_check(fw, "_CRS", "Resource Source Index", data[11], sizeof(data[11]), passed);
		break;
	default:
		snprintf(tmp, sizeof(tmp), "Method%sUnkownLargeResourceItem", objname);
		fwts_failed(fw, LOG_LEVEL_HIGH, tmp,
			"%s tag bits 6:0 is an undefined "
			"large tag item name, value 0x%" PRIx8 ".",
			name, tag_item);
		fwts_advice(fw,
			"A large resource data type tag (byte 0 of the "
			"%s buffer) contains an undefined large tag "
			"item 'name'. The %s buffer is therefore "
			"undefined and can't be used.  See section "
			"'6.4.3 Large Resource Data Type' of the ACPI "
			"specification, and also table 6-173.",
			objname, objname);
		*passed = false;
		break;
	}

	*tag = types[tag_item < 16 ? tag_item : 0];
}


#endif