/* ps_input_report_reader.h
 * Structured reader for generic input reports, eg from HID.
 * TODO We don't support collection fields, like for HID keyboards. Should we? Our focus is joysticks.
 */

#ifndef PS_INPUT_REPORT_READER_H
#define PS_INPUT_REPORT_READER_H

struct ps_input_report_reader;

struct ps_input_report_reader *ps_input_report_reader_new();
void ps_input_report_reader_del(struct ps_input_report_reader *reader);
int ps_input_report_reader_ref(struct ps_input_report_reader *reader);

/* Setup.
 ***************************************************************/

/* Default order is little-endian '<'.
 * Use '>' for big-endian.
 */
int ps_input_report_reader_set_byte_order(struct ps_input_report_reader *reader,char order);

/* Add unused bits to schema at the current insertion point.
 */
int ps_input_report_reader_add_padding(struct ps_input_report_reader *reader,int bitc);
int ps_input_report_reader_pad_to_multiple(struct ps_input_report_reader *reader,int multiple);

/* Add a field to the schema.
 * (size) must be in 1..32.
 * If (sign) nonzero and (size>1), the MSB is a sign bit.
 * (fldid) is an arbitrary identifier.
 * We don't care if you use duplicate (fldid). The first will be the most accessible.
 * Returns fldix on success, index of this field starting at zero.
 */
int ps_input_report_reader_add_field(
  struct ps_input_report_reader *reader,
  int size,int sign,
  int fldid
);

/* Add field without considering insertion point.
 * This will happily reuse bits already apportioned to another field. (That's your problem, not mine)
 * If this field exceeds the current insertion point, we update insertion point to the end.
 * Otherwise we leave it alone.
 */
int ps_input_report_reader_add_field_at(
  struct ps_input_report_reader *reader,
  int position,int size,int sign,
  int fldid
);

/* Forbid any further schema changes, and allow report processing to begin.
 */
int ps_input_report_reader_finish_setup(struct ps_input_report_reader *reader);

/* Examination.
 ****************************************************************/

/* Nonzero if the schema is finalized and reports can be delivered.
 */
int ps_input_report_reader_is_ready(const struct ps_input_report_reader *reader);

/* Length in bytes of expected input reports.
 */
int ps_input_report_reader_report_length(const struct ps_input_report_reader *reader);

/* Read current logical state, or schema.
 * It is more efficient to use (fldix) if you can.
 * (value) comes from the most recent report, initially zero, and zero for undefined fields.
 */
int ps_input_report_reader_count_fields(const struct ps_input_report_reader *reader);
int ps_input_report_reader_fldix_by_fldid(const struct ps_input_report_reader *reader,int fldid);
int ps_input_report_reader_fldid_by_fldix(const struct ps_input_report_reader *reader,int fldix);
int ps_input_report_reader_position_by_fldix(const struct ps_input_report_reader *reader,int fldix);
int ps_input_report_reader_size_by_fldix(const struct ps_input_report_reader *reader,int fldix);
int ps_input_report_reader_sign_by_fldix(const struct ps_input_report_reader *reader,int fldix);
int ps_input_report_reader_value_by_fldix(const struct ps_input_report_reader *reader,int fldix);

/* Maintenance.
 ****************************************************************/

/* Deliver a report to the reader and trigger your callback for any changes.
 * It's OK to not provide a callback.
 * It's OK if report length doesn't match; we zero-pad or truncate as necessary.
 * Please note that in case of a short input, we don't ignore the OOB fields, we zero them.
 * Comparison stops at the first nonzero result from (cb) and returns the same.
 * The new report is retained as "current state", even if you abort via callback.
 */
int ps_input_report_reader_set_report(
  struct ps_input_report_reader *reader,
  const void *src,int srcc,
  void *userdata,
  int (*cb)(struct ps_input_report_reader *reader,int fldix,int fldid,int value,int prevvalue,void *userdata)
);

#endif
