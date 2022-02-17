#pragma once

#include <generated/data.h>

// Defaults

extern ConsumerConfiguration
defaultConsumerConfiguration();

extern ProducerConfiguration
defaultProducerConfiguration();

extern ServerConfiguration
defaultServerConfiguration();

extern Configuration
defaultConfiguration();

extern AllConfiguration
defaultAllConfiguration();

// Printing

extern int
printAddress(const Address*);

extern int
printAllConfiguration(const AllConfiguration*);

extern int
printConsumerConfiguration(const ConsumerConfiguration*);

extern int
printProducerConfiguration(const ProducerConfiguration*);

extern int
printServerConfiguration(const ServerConfiguration*);

// Parsing

extern bool
parseProducerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseConsumerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseServerConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseAllConfiguration(const int, const char**, pAllConfiguration);

extern bool
parseCommonConfiguration(const char*, const char*, pAllConfiguration);

extern bool
parseAddress(const char*, pAddress);

// Run

extern int
runApp(const int, const char**);

extern int
runConsumer(const AllConfiguration*);

extern int
runServer(const AllConfiguration*);

extern int
runProducer(const AllConfiguration*);

// Parse failures

extern void
parseFailure(const char* type, const char* arg1, const char* arg2);