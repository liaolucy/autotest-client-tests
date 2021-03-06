/*
 * (C) Copyright IBM Corp. 2004, 2005
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 *
 * Authors:
 *     Carl McAdams <carlmc@us.ibm.com>
 *     Liang Daming <daming.liang@intel.com>
 *
 * Spec:        HPI-B.01.01
 * Function:    saHpiEventLogEntryAdd
 * Description:   
 *   For Event logs which support OVERFLOW_OVERWRITE overflow action,  
 *   add entries until the overflow is exceeded.
 *   saHpiEventLogEntryAdd() returns SA_OK, and the new event log 
 *   entry is present in the log.
 * Line:        P52-1:P52-10
 *
 */
#include <stdio.h>
#include <string.h>
#include "saf_test.h"

#define TEST_STRING     "Event Test"
#define TEST_STRING_LENGTH 10

#define SPECIAL_STR "Special Event Message"
#define SPECIAL_LENGTH (strlen(SPECIAL_STR))

SaHpiBoolT is_special_event(SaHpiEventLogEntryT * eventLogEntry)
{
	return (eventLogEntry->Event.EventType == SAHPI_ET_USER) &&
	    (eventLogEntry->Event.EventDataUnion.UserEvent.UserEventData.
	     DataLength == SPECIAL_LENGTH)
	    &&
	    (!memcmp
	     (eventLogEntry->Event.EventDataUnion.UserEvent.UserEventData.Data,
	      SPECIAL_STR, SPECIAL_LENGTH));
}

int overflow_test(SaHpiSessionIdT session, SaHpiResourceIdT resourceId)
{
	SaHpiEventLogInfoT info;
	SaErrorT status;
	SaHpiUint32T index, size;
	SaHpiEventT testevent;
	SaHpiEventLogEntryIdT PrevEntryId, NextEntryId;
	//SaHpiEventLogEntryIdT   current_entry_id;
	SaHpiEventLogEntryT EventLogEntry;
	int retval = SAF_TEST_UNKNOWN;
	SaHpiBoolT LogFilled = SAHPI_FALSE;
	SaHpiBoolT old_log_state;
	SaHpiBoolT restore_log_state = SAHPI_TRUE;

	status = saHpiEventLogStateGet(session, resourceId, &old_log_state);
	if (status != SA_OK) {
		retval = SAF_TEST_UNRESOLVED;
		e_print(saHpiEventLogStateGet, SA_OK, status);
	}
	//Disable eventlog

	if (retval == SAF_TEST_UNKNOWN && old_log_state) {
		status =
		    saHpiEventLogStateSet(session, resourceId, SAHPI_FALSE);
		if (status == SA_OK) {
			restore_log_state = SAHPI_TRUE;
		} else {
			retval = SAF_TEST_UNRESOLVED;
			e_print(saHpiEventLogStateSet, SA_OK, status);
		}
	}

	if (retval == SAF_TEST_UNKNOWN) {
		status = saHpiEventLogInfoGet(session, resourceId, &info);
		if (status != SA_OK) {
			e_print(saHpiEventLogInfoGet, SA_OK, status);
			retval = SAF_TEST_UNRESOLVED;
		} else {
			size = info.Size;
			//Event log exist, but is always empty 
			if ((size == 0) ||
			    (info.OverflowAction !=
			     SAHPI_EL_OVERFLOW_OVERWRITE)) {
				// If the size is not real or if the overflow
				// behavior is not fitting for this test, 
				// then just pass this resource or domain.
				retval = SAF_TEST_NOTSUPPORT;
			} else
				LogFilled = SAHPI_TRUE;
		}
	}
	// Clear the eventlog
	if (retval == SAF_TEST_UNKNOWN) {
		// Not only should this clear the event log,
		// but the overflow flag should be reset.
		status = saHpiEventLogClear(session, resourceId);
		if (status != SA_OK) {
			// return a bad status if unable to clear.
			m_print
			    ("\"saHpiEventLogClear\" failed to clear the eventlog! Return value: %s",
			     get_error_string(status));
			retval = SAF_TEST_UNRESOLVED;
		}
	}
	// Create the first entry in the eventlog
	if (retval == SAF_TEST_UNKNOWN) {
		testevent.EventType = SAHPI_ET_USER;
		testevent.Source = SAHPI_UNSPECIFIED_RESOURCE_ID;
		testevent.Timestamp = SAHPI_TIME_UNSPECIFIED;
		testevent.Severity = SAHPI_OK;
		strncpy(testevent.EventDataUnion.UserEvent.UserEventData.Data,
			TEST_STRING, TEST_STRING_LENGTH);
		testevent.EventDataUnion.UserEvent.UserEventData.DataLength =
		    TEST_STRING_LENGTH;
		testevent.EventDataUnion.UserEvent.UserEventData.Language =
		    SAHPI_LANG_ENGLISH;
		testevent.EventDataUnion.UserEvent.UserEventData.DataType =
		    SAHPI_TL_TYPE_TEXT;

		status = saHpiEventLogEntryAdd(session, resourceId, &testevent);
		if (status != SA_OK) {
			e_print(saHpiEventLogEntryAdd, SA_OK, status);
			retval = SAF_TEST_UNRESOLVED;
		}
	}
	// Fill up the rest of the eventlog
	if (retval == SAF_TEST_UNKNOWN) {
		for (index = 1; index < info.Size; index++) {
			status = saHpiEventLogEntryAdd(session,
						       resourceId, &testevent);
			if (status != SA_OK) {
				e_print(saHpiEventLogEntryAdd, SA_OK, status);
				retval = SAF_TEST_UNRESOLVED;
				break;
			}
		}
	}
	// test to make sure the eventlog is full
	if (retval == SAF_TEST_UNKNOWN) {

		status = saHpiEventLogInfoGet(session, resourceId, &info);
		if (status != SA_OK) {
			e_print(saHpiEventLogInfoGet, SA_OK, status);
			retval = SAF_TEST_UNRESOLVED;
		} else {
			if (info.Entries != info.Size) {
				m_print("EventLog was not filled up!");
				retval = SAF_TEST_UNRESOLVED;
			}
		}
	}
	// Add another entry to overflow the maximum limit
	if (retval == SAF_TEST_UNKNOWN) {
		testevent.EventType = SAHPI_ET_USER;
		testevent.Severity = SAHPI_INFORMATIONAL;
		testevent.EventDataUnion.UserEvent.UserEventData.DataLength =
		    SPECIAL_LENGTH;
		memcpy(testevent.EventDataUnion.UserEvent.UserEventData.Data,
		       SPECIAL_STR, SPECIAL_LENGTH);

		status = saHpiEventLogEntryAdd(session, resourceId, &testevent);
		if (status != SA_OK) {
			e_print(saHpiEventLogEntryAdd, SA_OK, status);
			retval = SAF_TEST_FAIL;
		}
	}
	// Retrieve the added test event to make sure that it was not dropped.
	if (retval == SAF_TEST_UNKNOWN) {
		status = saHpiEventLogEntryGet(session,
					       resourceId,
					       SAHPI_NEWEST_ENTRY,
					       &PrevEntryId,
					       &NextEntryId,
					       &EventLogEntry, NULL, NULL);
		if (status == SA_ERR_HPI_NOT_PRESENT) {
			retval = SAF_TEST_NOTSUPPORT;
		} else if (status != SA_OK) {
			e_print(saHpiEventLogEntryGet, SA_OK, status);
			retval = SAF_TEST_UNRESOLVED;
		}
	}

	if (retval == SAF_TEST_UNKNOWN) {
		if (is_special_event(&EventLogEntry)) {
			retval = SAF_TEST_PASS;
		} else {
			// This event severity was set on only the first event entry
			m_print
			    ("\"saHpiEventLogInfoAdd\" the added event does not exist!");
			retval = SAF_TEST_FAIL;
		}
	}
	// clean up
	if (LogFilled != SAHPI_FALSE)
		status = saHpiEventLogClear(session, resourceId);

	// Restore eventlog state

	if (restore_log_state) {
		status =
		    saHpiEventLogStateSet(session, resourceId, old_log_state);
		if (status != SA_OK) {
			e_print(saHpiEventLogStateSet, SA_OK, status);
		}
	}

	return retval;
}

int testcase_domain(SaHpiSessionIdT session)
{
	int retval = SAF_TEST_UNKNOWN;

	// Test the domain event log
	retval = overflow_test(session, SAHPI_UNSPECIFIED_RESOURCE_ID);

	return retval;
}

int Test_Resource(SaHpiSessionIdT session,
		  SaHpiRptEntryT report, callback2_t func)
{
	int retval = SAF_TEST_UNKNOWN;

	if (report.ResourceCapabilities & SAHPI_CAPABILITY_EVENT_LOG)
		retval = overflow_test(session, report.ResourceId);
	else {
		//Resource does not support Event logs
		retval = SAF_TEST_NOTSUPPORT;
	}
	return retval;
}

int main()
{
	int retval = SAF_TEST_UNKNOWN;

	retval = process_all_domains(Test_Resource, NULL, testcase_domain);

	return retval;
}
