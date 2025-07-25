<?php

/** @generate-class-entries */

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC3339
 */
const DATE_ATOM = "Y-m-d\\TH:i:sP";

/**
 * @var string
 * @cvalue DATE_FORMAT_COOKIE
 */
const DATE_COOKIE = "l, d-M-Y H:i:s T";

/**
 * @var string
 * @cvalue DATE_FORMAT_ISO8601
 */
const DATE_ISO8601 = "Y-m-d\\TH:i:sO";

/**
 * @var string
 * @cvalue DATE_FORMAT_ISO8601_EXPANDED
 */
const DATE_ISO8601_EXPANDED = "X-m-d\\TH:i:sP";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC822
 */
const DATE_RFC822 = "D, d M y H:i:s O";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC850
 */
const DATE_RFC850 = "l, d-M-y H:i:s T";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC1036
 */
const DATE_RFC1036 = "D, d M y H:i:s O";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC1123
 */
const DATE_RFC1123 = "D, d M Y H:i:s O";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC7231
 */
const DATE_RFC7231 = "D, d M Y H:i:s \\G\\M\\T";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC2822
 */
const DATE_RFC2822 = "D, d M Y H:i:s O";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC3339
 */
const DATE_RFC3339 = "Y-m-d\\TH:i:sP";

/**
 * @var string
 * @cvalue DATE_FORMAT_RFC3339_EXTENDED
 */
const DATE_RFC3339_EXTENDED = "Y-m-d\\TH:i:s.vP";

/** @var string */
const DATE_RSS = DATE_RFC1123;

/** @var string */
const DATE_W3C = DATE_RFC3339;

/**
 * @var int
 * @cvalue SUNFUNCS_RET_TIMESTAMP
 */
#[\Deprecated(since: '8.4', message: 'as date_sunrise() and date_sunset() were deprecated in 8.1')]
const SUNFUNCS_RET_TIMESTAMP = UNKNOWN;

/**
 * @var int
 * @cvalue SUNFUNCS_RET_STRING
 */
#[\Deprecated(since: '8.4', message: 'as date_sunrise() and date_sunset() were deprecated in 8.1')]
const SUNFUNCS_RET_STRING = UNKNOWN;

/**
 * @var int
 * @cvalue SUNFUNCS_RET_DOUBLE
 */
#[\Deprecated(since: '8.4', message: 'as date_sunrise() and date_sunset() were deprecated in 8.1')]
const SUNFUNCS_RET_DOUBLE = UNKNOWN;

function strtotime(string $datetime, ?int $baseTimestamp = null): int|false {}

/** @refcount 1 */
function date(string $format, ?int $timestamp = null): string {}

function idate(string $format, ?int $timestamp = null): int|false {}

/** @refcount 1 */
function gmdate(string $format, ?int $timestamp = null): string {}

function mktime(
    int $hour, ?int $minute = null, ?int $second = null,
    ?int $month = null, ?int $day = null, ?int $year = null): int|false {}

function gmmktime(
    int $hour, ?int $minute = null, ?int $second = null,
    ?int $month = null, ?int $day = null, ?int $year = null): int|false {}

function checkdate(int $month, int $day, int $year): bool {}

/**
 * @refcount 1
 */
#[\Deprecated(since: '8.1', message: 'use IntlDateFormatter::format() instead')]
function strftime(string $format, ?int $timestamp = null): string|false {}

/**
 * @refcount 1
 */
#[\Deprecated(since: '8.1', message: 'use IntlDateFormatter::format() instead')]
function gmstrftime(string $format, ?int $timestamp = null): string|false {}

function time(): int {}

/**
 * @return array<int|string, int>
 * @refcount 1
 */
function localtime(?int $timestamp = null, bool $associative = false): array {}

/**
 * @return array<int|string, int|string>
 * @refcount 1
 */
function getdate(?int $timestamp = null): array {}

/** @refcount 1 */
function date_create(string $datetime = "now", ?DateTimeZone $timezone = null): DateTime|false {}

/** @refcount 1 */
function date_create_immutable(
    string $datetime = "now", ?DateTimeZone $timezone = null): DateTimeImmutable|false {}

/** @refcount 1 */
function date_create_from_format(
    string $format, string $datetime, ?DateTimeZone $timezone = null): DateTime|false {}

/** @refcount 1 */
function date_create_immutable_from_format(
    string $format, string $datetime, ?DateTimeZone $timezone = null): DateTimeImmutable|false {}

/**
 * @return array<string, mixed>
 * @refcount 1
 */
function date_parse(string $datetime): array {}

/**
 * @return array<string, mixed>
 * @refcount 1
 */
function date_parse_from_format(string $format, string $datetime): array {}

/**
 * @return array<string, int|array>|false
 * @refcount 1
 */
function date_get_last_errors(): array|false {}

/** @refcount 1 */
function date_format(DateTimeInterface $object, string $format): string {}

function date_modify(DateTime $object, string $modifier): DateTime|false {}

function date_add(DateTime $object, DateInterval $interval): DateTime {}

function date_sub(DateTime $object, DateInterval $interval): DateTime {}

/** @refcount 1 */
function date_timezone_get(DateTimeInterface $object): DateTimeZone|false {}

function date_timezone_set(DateTime $object, DateTimeZone $timezone): DateTime {}

function date_offset_get(DateTimeInterface $object): int {}

/** @refcount 1 */
function date_diff(
    DateTimeInterface $baseObject, DateTimeInterface $targetObject, bool $absolute = false): DateInterval {}

function date_time_set(
    DateTime $object, int $hour, int $minute, int $second = 0, int $microsecond = 0): DateTime {}

function date_date_set(DateTime $object, int $year, int $month, int $day): DateTime {}

function date_isodate_set(DateTime $object, int $year, int $week, int $dayOfWeek = 1): DateTime {}

function date_timestamp_set(DateTime $object, int $timestamp): DateTime {}

function date_timestamp_get(DateTimeInterface $object): int {}

/** @refcount 1 */
function timezone_open(string $timezone): DateTimeZone|false {}

/** @refcount 1 */
function timezone_name_get(DateTimeZone $object): string {}

/** @refcount 1 */
function timezone_name_from_abbr(string $abbr, int $utcOffset = -1, int $isDST = -1): string|false {}

function timezone_offset_get(DateTimeZone $object, DateTimeInterface $datetime): int {}

/**
 * @return array<int, array>|false
 * @refcount 1
 */
function timezone_transitions_get(
    DateTimeZone $object, int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647): array|false {}

/**
 * @return array<string, float|string>|false
 * @refcount 1
 */
function timezone_location_get(DateTimeZone $object): array|false {}

/**
 * @return array<int, string>
 * @refcount 1
 */
function timezone_identifiers_list(int $timezoneGroup = DateTimeZone::ALL, ?string $countryCode = null): array {}

/**
 * @return array<string, array>
 * @refcount 1
 */
function timezone_abbreviations_list(): array {}

/** @refcount 1 */
function timezone_version_get(): string {}

/** @refcount 1 */
function date_interval_create_from_date_string(string $datetime): DateInterval|false {}

/** @refcount 1 */
function date_interval_format(DateInterval $object, string $format): string {}

function date_default_timezone_set(string $timezoneId): bool {}

/** @refcount 1 */
function date_default_timezone_get(): string {}

/**
 * @refcount 1
 */
#[\Deprecated(since: '8.1', message: 'use date_sun_info() instead')]
function date_sunrise(
    int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING,
    ?float $latitude = null, ?float $longitude = null, ?float $zenith = null,
    ?float $utcOffset = null): string|int|float|false {}

/**
 * @refcount 1
 */
#[\Deprecated(since: '8.1', message: 'use date_sun_info() instead')]
function date_sunset(
    int $timestamp, int $returnFormat = SUNFUNCS_RET_STRING,
    ?float $latitude = null, ?float $longitude = null, ?float $zenith = null,
    ?float $utcOffset = null): string|int|float|false {}

/**
 * @return array<string, bool|int>
 * @refcount 1
 */
function date_sun_info(int $timestamp, float $latitude, float $longitude): array {}

interface DateTimeInterface
{

    public const string ATOM = DATE_ATOM;

    public const string COOKIE = DATE_COOKIE;

    public const string ISO8601 = DATE_ISO8601;

    public const string ISO8601_EXPANDED = DATE_ISO8601_EXPANDED;

    public const string RFC822 = DATE_RFC822;

    public const string RFC850 = DATE_RFC850;

    public const string RFC1036 = DATE_RFC1036;

    public const string RFC1123 = DATE_RFC1123;

    public const string RFC7231 = DATE_RFC7231;

    public const string RFC2822 = DATE_RFC2822;

    public const string RFC3339 = DATE_RFC3339;

    public const string RFC3339_EXTENDED = DATE_RFC3339_EXTENDED;

    public const string RSS = DATE_RSS;

    public const string W3C = DATE_W3C;

    /** @tentative-return-type */
    public function format(string $format): string;

    /** @tentative-return-type */
    public function getTimezone(): DateTimeZone|false;

    /** @tentative-return-type */
    public function getOffset(): int;

    /** @tentative-return-type */
    public function getTimestamp(): int;

    public function getMicrosecond(): int;

    /** @tentative-return-type */
    public function diff(DateTimeInterface $targetObject, bool $absolute = false): DateInterval;

    /** @tentative-return-type */
    public function __wakeup(): void;

    public function __serialize(): array;

    public function __unserialize(array $data): void;
}

class DateTime implements DateTimeInterface
{
    public function __construct(string $datetime = "now", ?DateTimeZone $timezone = null) {}

    public function __serialize(): array {}

    public function __unserialize(array $data): void {}

    /** @tentative-return-type */
    public function __wakeup(): void {}

    /** @tentative-return-type */
    public static function __set_state(array $array): DateTime {}

    /** @tentative-return-type */
    public static function createFromImmutable(DateTimeImmutable $object): static {}

    /** @return static */
    public static function createFromInterface(DateTimeInterface $object): DateTime {} // TODO return type should be static

    /**
     * @tentative-return-type
     * @alias date_create_from_format
     */
    public static function createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null): DateTime|false {}

    /** @tentative-return-type */
    public static function createFromTimestamp(int|float $timestamp): static {}

    /**
     * @return array<string, int|array>|false
     * @tentative-return-type
     * @alias date_get_last_errors
     */
    public static function getLastErrors(): array|false {}

    /**
     * @tentative-return-type
     * @alias date_format
     */
    public function format(string $format): string {}

    /**
     * @tentative-return-type
     */
    public function modify(string $modifier): DateTime {}

    /**
     * @tentative-return-type
     * @alias date_add
     */
    public function add(DateInterval $interval): DateTime {}

    /**
     * @tentative-return-type
     */
    public function sub(DateInterval $interval): DateTime {}

    /**
     * @tentative-return-type
     * @alias date_timezone_get
     */
    public function getTimezone(): DateTimeZone|false {}

    /**
     * @tentative-return-type
     * @alias date_timezone_set
     */
    public function setTimezone(DateTimeZone $timezone): DateTime {}

    /**
     * @tentative-return-type
     * @alias date_offset_get
     */
    public function getOffset(): int {}

    public function getMicrosecond(): int {}

    /**
     * @tentative-return-type
     * @alias date_time_set
     */
    public function setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0): DateTime {}

    /**
     * @tentative-return-type
     * @alias date_date_set
     */
    public function setDate(int $year, int $month, int $day): DateTime {}

    /**
     * @tentative-return-type
     * @alias date_isodate_set
     */
    public function setISODate(int $year, int $week, int $dayOfWeek = 1): DateTime {}

    /**
     * @tentative-return-type
     * @alias date_timestamp_set
     */
    public function setTimestamp(int $timestamp): DateTime {}

    public function setMicrosecond(int $microsecond): static {}

    /**
     * @tentative-return-type
     * @alias date_timestamp_get
     */
    public function getTimestamp(): int {}

    /**
     * @tentative-return-type
     * @alias date_diff
     */
    public function diff(DateTimeInterface $targetObject, bool $absolute = false): DateInterval {}
}

class DateTimeImmutable implements DateTimeInterface
{
    public function __construct(string $datetime = "now", ?DateTimeZone $timezone = null) {}

    public function __serialize(): array {}

    public function __unserialize(array $data): void {}

    /** @tentative-return-type */
    public function __wakeup(): void {}

    /** @tentative-return-type */
    public static function __set_state(array $array): DateTimeImmutable {}

    /**
     * @tentative-return-type
     * @alias date_create_immutable_from_format
     */
    public static function createFromFormat(string $format, string $datetime, ?DateTimeZone $timezone = null): DateTimeImmutable|false {}

    /** @tentative-return-type */
    public static function createFromTimestamp(int|float $timestamp): static {}

    /**
     * @return array<string, int|array>|false
     * @tentative-return-type
     * @alias date_get_last_errors
     */
    public static function getLastErrors(): array|false {}

    /**
     * @tentative-return-type
     * @alias date_format
     */
    public function format(string $format): string {}

    /**
     * @tentative-return-type
     * @alias date_timezone_get
     */
    public function getTimezone(): DateTimeZone|false {}

    /**
     * @tentative-return-type
     * @alias date_offset_get
     */
    public function getOffset(): int {}

    /**
     * @tentative-return-type
     * @alias date_timestamp_get
     */
    public function getTimestamp(): int {}

    /**
     * @alias DateTime::getMicrosecond
     */
    public function getMicrosecond(): int {}

    /**
     * @tentative-return-type
     * @alias date_diff
     */
    public function diff(DateTimeInterface $targetObject, bool $absolute = false): DateInterval {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::modify() does not modify the object itself")]
    public function modify(string $modifier): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::add() does not modify the object itself")]
    public function add(DateInterval $interval): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::sub() does not modify the object itself")]
    public function sub(DateInterval $interval): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::setTimezone() does not modify the object itself")]
    public function setTimezone(DateTimeZone $timezone): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::setTime() does not modify the object itself")]
    public function setTime(int $hour, int $minute, int $second = 0, int $microsecond = 0): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::setDate() does not modify the object itself")]
    public function setDate(int $year, int $month, int $day): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::setISODate() does not modify the object itself")]
    public function setISODate(int $year, int $week, int $dayOfWeek = 1): DateTimeImmutable {}

    /** @tentative-return-type */
    #[\NoDiscard(message: "as DateTimeImmutable::setTimestamp() does not modify the object itself")]
    public function setTimestamp(int $timestamp): DateTimeImmutable {}

    #[\NoDiscard(message: "as DateTimeImmutable::setMicrosecond() does not modify the object itself")]
    public function setMicrosecond(int $microsecond): static {}

    /** @tentative-return-type */
    public static function createFromMutable(DateTime $object): static {}

    /** @return static */
    public static function createFromInterface(DateTimeInterface $object): DateTimeImmutable {} // TODO return type should be static
}

class DateTimeZone
{
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_AFRICA */
    public const int AFRICA = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_AMERICA */
    public const int AMERICA = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_ANTARCTICA */
    public const int ANTARCTICA = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_ARCTIC */
    public const int ARCTIC = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_ASIA */
    public const int ASIA = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_ATLANTIC */
    public const int ATLANTIC = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_AUSTRALIA */
    public const int AUSTRALIA = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_EUROPE */
    public const int EUROPE = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_INDIAN */
    public const int INDIAN = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_PACIFIC */
    public const int PACIFIC = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_UTC */
    public const int UTC = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_ALL */
    public const int ALL = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_GROUP_ALL_W_BC */
    public const int ALL_WITH_BC = UNKNOWN;
    /** @cvalue PHP_DATE_TIMEZONE_PER_COUNTRY */
    public const int PER_COUNTRY = UNKNOWN;

    public function __construct(string $timezone) {}

    /**
     * @tentative-return-type
     * @alias timezone_name_get
     */
    public function getName(): string {}

    /**
     * @tentative-return-type
     * @alias timezone_offset_get
     */
    public function getOffset(DateTimeInterface $datetime): int {}

    /**
     * @return array<int, array>|false
     * @tentative-return-type
     * @alias timezone_transitions_get
     */
    public function getTransitions(int $timestampBegin = PHP_INT_MIN, int $timestampEnd = 2147483647): array|false {}

    /**
     * @return array<string, float|string>|false
     * @tentative-return-type
     * @alias timezone_location_get
     */
    public function getLocation(): array|false {}

    /**
     * @return array<string, array>
     * @tentative-return-type
     * @alias timezone_abbreviations_list
     */
    public static function listAbbreviations(): array {}

    /**
     * @return array<int, string>
     * @tentative-return-type
     * @alias timezone_identifiers_list
     */
    public static function listIdentifiers(int $timezoneGroup = DateTimeZone::ALL, ?string $countryCode = null): array {}

    public function __serialize(): array {}

    public function __unserialize(array $data): void {}

    /** @tentative-return-type */
    public function __wakeup(): void {}

    /** @tentative-return-type */
    public static function __set_state(array $array): DateTimeZone {}
}

class DateInterval
{
    public function __construct(string $duration) {}

    /**
     * @tentative-return-type
     */
    public static function createFromDateString(string $datetime): DateInterval {}

    /**
     * @tentative-return-type
     * @alias date_interval_format
     */
    public function format(string $format): string {}

    public function __serialize(): array;

    public function __unserialize(array $data): void;

    /** @tentative-return-type */
    public function __wakeup(): void {}

    /** @tentative-return-type */
    public static function __set_state(array $array): DateInterval {}
}

class DatePeriod implements IteratorAggregate
{
    /** @cvalue PHP_DATE_PERIOD_EXCLUDE_START_DATE */
    public const int EXCLUDE_START_DATE = UNKNOWN;
    /** @cvalue PHP_DATE_PERIOD_INCLUDE_END_DATE */
    public const int INCLUDE_END_DATE = UNKNOWN;

    /**
     * @readonly
     * @virtual
     */
    public ?DateTimeInterface $start;
    /**
     * @readonly
     * @virtual
     */
    public ?DateTimeInterface $current;
    /**
     * @readonly
     * @virtual
     */
    public ?DateTimeInterface $end;
    /**
     * @readonly
     * @virtual
     */
    public ?DateInterval $interval;
    /**
     * @readonly
     * @virtual
     */
    public int $recurrences;
    /**
     * @readonly
     * @virtual
     */
    public bool $include_start_date;
    /**
     * @readonly
     * @virtual
     */
    public bool $include_end_date;

    public static function createFromISO8601String(string $specification, int $options = 0): static {}

    /**
     * @param DateTimeInterface|string $start
     * @param DateInterval|int $interval
     * @param DateTimeInterface|int $end
     * @param int $options
     */
    public function __construct($start, $interval = UNKNOWN, $end = UNKNOWN, $options = UNKNOWN) {}

    /** @tentative-return-type */
    public function getStartDate(): DateTimeInterface {}

    /** @tentative-return-type */
    public function getEndDate(): ?DateTimeInterface {}

    /** @tentative-return-type */
    public function getDateInterval(): DateInterval {}

    /** @tentative-return-type */
    public function getRecurrences(): ?int {}

    public function __serialize(): array;

    public function __unserialize(array $data): void;

    /** @tentative-return-type */
    public function __wakeup(): void {}

    /** @tentative-return-type */
    public static function __set_state(array $array): DatePeriod {}

    public function getIterator(): Iterator {}
}

/**
 * @strict-properties
 */
class DateError extends Error
{
}

/**
 * @strict-properties
 */
class DateObjectError extends DateError
{
}

/**
 * @strict-properties
 */
class DateRangeError extends DateError
{
}

/**
 * @strict-properties
 */
class DateException extends Exception
{
}

/**
 * @strict-properties
 */
class DateInvalidTimeZoneException extends DateException
{
}

/**
 * @strict-properties
 */
class DateInvalidOperationException extends DateException
{
}

/**
 * @strict-properties
 */
class DateMalformedStringException extends DateException
{
}

/**
 * @strict-properties
 */
class DateMalformedIntervalStringException extends DateException
{
}

/**
 * @strict-properties
 */
class DateMalformedPeriodStringException extends DateException
{
}
