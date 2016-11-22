/*
 * Copyright (C) 2016 Mountassir El Hafi, (mountassirbillah1@gmail.com)
 *
 * Manager.cpp: Part of sspender
 *
 * sspender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * sspender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with sspender.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Manager.h"

void Manager::setWhatToMonitor(bool suspendIfCpuIdle, bool suspendIfStorageIdle)
{
	m_suspendIfCpuIdle = suspendIfCpuIdle;
	m_suspendIfStorageIdle = suspendIfStorageIdle;
}

void Manager::setIpsToWatch(const vector<string> &ipToWatch)
{
	for(size_t i = 0, size = ipToWatch.size(); i < size; ++i)
	{
		m_ipsToWatch.push_back(ipToWatch[i]);
	}
}

void Manager::setDisksToMonitor(const vector<DiskCfg> &disksToMonitor)
{
	for(size_t i = 0, size = disksToMonitor.size(); i < size; ++i)
	{
		m_disksToMonitor.push_back(disksToMonitor[i]);
	}
}

void Manager::setCpusToMonitor()
{
	string cpu = "cpu";

	m_cpusToMonitor.push_back(cpu);
}

void Manager::setTimesToWakeAt(const vector<string> &wakeAt)
{
	for(size_t i = 0, size = wakeAt.size(); i < size; ++i)
	{
		m_timesToWakeAt.push_back(wakeAt[i]);
	}
}

void Manager::setSleepMode(SLEEP_MODE sleepMode)
{
	m_sleepMode = sleepMode;
}

void Manager::setTimers(int check_if_idle_every,
				   	    int stop_monitoring_for,
				   	    int reset_monitoring_after,
				   	    int suspend_after)
{
	m_checkIfIdleEvery     = check_if_idle_every;
	m_stopMonitoringFor    = stop_monitoring_for;
	m_resetMonitoringAfter = reset_monitoring_after;
	m_suspendAfter         = suspend_after;
}

void Manager::monitorSystemUsage()
{
	int idleTimer = 0;
	int notIdleTimer = 0;

	//start monitoring the usage of the passed in devices
	m_monitor.monitorSystemUsage(m_disksToMonitor, m_cpusToMonitor);

	while(true)
	{
		printHeaderMessage("Checking if clients are online", true);

		bool stayOnline = m_monitor.areClientsConnected(m_ipsToWatch);

		if(stayOnline)
		{
			cout << "Found clients online, will stop monitoring for "
				 << m_stopMonitoringFor
				 << " mins." << endl;

			//if any of the specified IPs is online, reset the counters and
			//stop checking if the machine is idle, note that the usage of
			//the devices will still be monitored by the detached threads
			idleTimer = 0;
			notIdleTimer = 0;

			std::this_thread::sleep_for(std::chrono::minutes(m_stopMonitoringFor));

			continue;
		}

		printHeaderMessage("Checking if system is idle", true);

		if(canBeSuspended())
		{
			bool isIdle = isTheMachineIdle();

			if(isIdle)
			{
				cout << "System is idle (" << idleTimer << ").\n";

				notIdleTimer = 0;
				++idleTimer;
			}
			else
			{
				cout << "System is not idle (" << notIdleTimer << ").\n";

				++notIdleTimer;
			}

			int minutesTheMachineBeenBusyFor = notIdleTimer * m_checkIfIdleEvery;

			//if system is busy for # minutes
			if(minutesTheMachineBeenBusyFor > m_resetMonitoringAfter)
			{
				cout << "System was busy for more than " << m_resetMonitoringAfter
					 << " mins, reseting idle timer.\n";

				idleTimer = 0;
				notIdleTimer = 0;
			}

			int minutesTheMachineBeenIdleFor = idleTimer * m_checkIfIdleEvery;

			//if idle for # minutes
			if(minutesTheMachineBeenIdleFor > m_suspendAfter)
			{
				cout << "system was idle for more than "
					 << m_suspendAfter
					 << " mins, will suspend the machine.\n";

				idleTimer = 0;
				notIdleTimer = 0;

				printHeaderMessage("Suspending the machine", true);

				suspendTheMachine();
			}
		}
		else
		{
			double cpuLoad, storageLoad, storageRead, storageWritten;

			getTheMachineUsage(&cpuLoad, &storageLoad, &storageRead, &storageWritten);
			printTheMachineUsage(cpuLoad, storageLoad, storageRead, storageWritten);
		}

		//check if the machine is idle every # minutes
		std::this_thread::sleep_for(std::chrono::minutes(m_checkIfIdleEvery));
	}
}

void Manager::getTheMachineUsage(double *cpuLoad, double *storageLoad, double *storageRead, double *storageWritten)
{
	m_monitor.getCpuLoad(cpuLoad);
	m_monitor.getStorageLoad(storageLoad, storageRead, storageWritten);
}

void Manager::printTheMachineUsage(double cpuLoad, double storageLoad, double storageRead, double storageWritten)
{
	cout << "Average CPU usage: Load - " << cpuLoad << "%." << "\n";

	cout << "Average Storage usage (across all monitored drives): Load - "
	     << storageLoad << "%, Read - " << storageRead << "KB/s, Written - "
	     << storageWritten << "KB/s.\n";
}

bool Manager::canBeSuspended()
{
	return m_suspendIfCpuIdle || m_suspendIfStorageIdle;
}

bool Manager::isTheMachineIdle()
{
	double cpuLoad, storageLoad, storageRead, storageWritten;

	getTheMachineUsage(&cpuLoad, &storageLoad, &storageRead, &storageWritten);
	printTheMachineUsage(cpuLoad, storageLoad, storageRead, storageWritten);

	bool isIdle = m_suspendIfCpuIdle || m_suspendIfStorageIdle;

	if(m_suspendIfCpuIdle)
	{
		if(cpuLoad > CPU_LIMIT)
		{
			cout << "CPU     -- busy.\n";

			isIdle = false;
		}
		else
		{
			cout << "CPU     -- idle.\n";
		}
	}
	else
	{
		cout << "suspend_if_cpu_idle is false, ignoring CPU usage.\n";
	}

	if(m_suspendIfStorageIdle)
	{
		if( (storageLoad > STORAGE_LOAD_LIMIT) ||
			(storageRead + storageWritten) > STORAGE_READ_WRITE_LIMIT)
		{
			cout << "Storage -- busy.\n";

			isIdle = false;
		}
		else
		{
			cout << "Storage -- idle.\n";
		}
	}
	else
	{
		cout << "suspend_if_storage_idle is false, ignoring storage usage.\n";
	}

	return isIdle;
}

void Manager::suspendTheMachine()
{
	double currentTimeInMinutes = 0;  //since 00:00

	getCurremtTimeInMinutes(&currentTimeInMinutes);

	vector<double> suspendUpTo;

	//convert all passed in times to minutes from 00:00
	//00:40 -> 40 minutes, 1:10 -> 70 minutes ...
	for(size_t i = 0, len = m_timesToWakeAt.size(); i < len; ++i)
	{
		double timeInMinutes;

		if(convertTimeToMinutes(m_timesToWakeAt[i], &timeInMinutes))
		{
			suspendUpTo.push_back(timeInMinutes);
		}
	}

	//sort by eraliest first
	sort(suspendUpTo.begin(), suspendUpTo.end(), sortVector);

	//find the earliest time within the same day
	for(size_t i = 0, len = suspendUpTo.size(); i < len; ++i)
	{
		if(currentTimeInMinutes < suspendUpTo[i])
		{
			suspendUntil(currentTimeInMinutes, suspendUpTo[i]);

			return;
		}
	}

	//if we hit hit code, that means that the earliest time in suspendUpTo is tomorrow
	//we will just pick the first element in the array as that is the smallest.
	if(suspendUpTo.size() > 0)
	{
		suspendUntil(currentTimeInMinutes, suspendUpTo[0]);
	}
	else
	{
		//todo suspend for ever
	}
}

void Manager::suspendUntil(double currentTimeInMinutes, double until)
{

	double secondsToSleep = 0;

	if(currentTimeInMinutes < until)
	{
		//suspend until time is on the same day
		secondsToSleep = ((until - currentTimeInMinutes) - SUSPEND_OFFSET) * SECONDS_IN_MINUTE;
	}
	else
	{
		//suspend until time is on the following day
		secondsToSleep = (until + (TOTAL_MINUTES_IN_DAY - currentTimeInMinutes) - SUSPEND_OFFSET) * SECONDS_IN_MINUTE;
	}

	cout << "Got: current time in minutes (" << currentTimeInMinutes << "), suspend until(" << until << ").\n";
	cout << "Suspending the machine for " << secondsToSleep << " seconds.\n";

	vector<string> output;

	rtcWakeSuspend(secondsToSleep, &output);
	//pmUtilSuspend(secondsToSleep, &output);

	//todo execute script after resume

	printHeaderMessage("System returned from suspend", true);
	cout << "Suspend output: ";

	for(size_t i = 0, len = output.size(); i < len; ++i)
	{
		cout << output[i] << " , ";
	}

	cout << "\n";
}

void Manager::rtcWakeSuspend(double secondsToSleep, vector<string> *output)
{
	string sleepMode = getRtcWakeSleepMode();

	ostringstream oss;
	oss << "rtcwake -m " << sleepMode << " -s " << secondsToSleep;

	runSystemCommand(oss.str(), output);
}

void Manager::pmUtilSuspend(double secondsToSleep, vector<string> *output)
{
	ostringstream oss;

	//Clear previously set wakeup time
	oss << "sh -c \"echo 0 > /sys/class/rtc/rtc0/wakealarm\"";

	runSystemCommand(oss.str());

	oss.str("");

	//Set the wakeup time
	oss << "sh -c \"echo `date '+%s' -d '+ " << (secondsToSleep / 60)
	    << " minutes'` > /sys/class/rtc/rtc0/wakealarm\"";

	runSystemCommand(oss.str());

	//After setting the time, PC can be turned off with this command
	runSystemCommand(getPmUtilCommand(), output);
}

string Manager::getRtcWakeSleepMode()
{
	switch(m_sleepMode)
	{
		case STAND_BY: { return string("standby");}
		case MEM:      { return string("mem");}
		case DISK:     { return string("disk");}
		default:       { return string("disk");}
	}
}

string Manager::getPmUtilCommand()
{
	switch(m_sleepMode)
	{
		case STAND_BY: { return string("pm-suspend");}
		case MEM:      { return string("pm-suspend");}
		case DISK:     { return string("pm-hibernate");}
		default:       { return string("pm-hibernate");}
	}
}
