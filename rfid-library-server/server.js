


// =======================================================
// 📚 LIBRARY SERVER (Unified + Gate Check)
// =======================================================
require("dotenv").config();
const express = require("express");
const fs = require("fs");
const path = require("path");
const nodemailer = require("nodemailer");
const cron = require("node-cron");

const app = express();
app.use(express.json());

const PORT = process.env.PORT || 3000;

// === FILE PATHS ===
const ISSUED_FILE = path.join(__dirname, "issued.json");
const ENTRY_FILE = path.join(__dirname, "entry_log.json");

// === LOAD EXISTING DATA ===
let issued = [];
let entryLogs = [];

try {
  // Load issued books
  if (fs.existsSync(ISSUED_FILE)) {
    const raw = JSON.parse(fs.readFileSync(ISSUED_FILE, "utf8"));

    // Convert object → array
    issued = Array.isArray(raw) ? raw : Object.values(raw);
  }

  // Load entry logs
  if (fs.existsSync(ENTRY_FILE)) {
    const raw2 = JSON.parse(fs.readFileSync(ENTRY_FILE, "utf8"));
    entryLogs = Array.isArray(raw2) ? raw2 : Object.values(raw2);
  }

} catch (err) {
  console.error("⚠️ Error loading saved data:", err);
}


// === SAVE HELPERS ===
function saveIssued() {
  fs.writeFileSync(ISSUED_FILE, JSON.stringify(issued, null, 2));
}
function saveEntries() {
  fs.writeFileSync(ENTRY_FILE, JSON.stringify(entryLogs, null, 2));
}

// === TIME HELPERS ===
function nowIST() {
  return new Date().toLocaleString("en-IN", { timeZone: "Asia/Kolkata" });
}
function parseDate(s) {
  if (!s) return null;
  const d = new Date(s);
  return isNaN(d.getTime()) ? null : d;
}
function addDays(date, days) {
  const d = new Date(date);
  d.setDate(d.getDate() + days);
  return d.toISOString();
}

// === EMAIL TRANSPORTER ===
const transporter = nodemailer.createTransport({
  host: process.env.SMTP_HOST,
  port: parseInt(process.env.SMTP_PORT, 10) || 465,
  secure: true,
  auth: {
    user: process.env.SMTP_USER,
    pass: process.env.SMTP_PASS,
  },
  tls: { rejectUnauthorized: false },
});

// EMAIL HELPER
async function sendEmail(to, subject, text) {
  const from = process.env.MAIL_FROM || process.env.SMTP_USER;
  try {
    const info = await transporter.sendMail({ from, to, subject, text });
    console.log(`Email sent to ${to}: ${info.response || info.messageId}`);
  } catch (err) {
    console.error(`Email error to ${to}:`, err.message || err);
  }
}

// =====================================================
// 📘 BOOK ISSUE
// =====================================================
app.post("/issue", async (req, res) => {
  const { student_uid, student_name, student_email, book_uid, book_title } = req.body;

  if (!student_uid || !book_uid)
    return res.status(400).json({ error: "student_uid and book_uid required" });

  const now = new Date().toISOString();
  const due = addDays(now, 7);

  const rec = {
    id: Date.now(),
    student_uid,
    student_name: student_name || "Unknown",
    student_email: student_email || null,
    book_uid,
    book_title: book_title || "Unknown",
    borrow_date: now,
    due_date: due,
    status: "Issued",
  };

  issued.push(rec);
  saveIssued();
  console.log(`ISSUED: ${student_name} → ${book_title}`);

  if (student_email) {
    const body =
      `Hello ${student_name},\n` +
      `You borrowed: ${book_title}\nDue date: ${due}`;
    await sendEmail(student_email, "Book Borrowed", body);
  }

  res.json({ ok: true, record: rec });
});

// =====================================================
// 📗 BOOK RETURN
// =====================================================
app.post("/return", async (req, res) => {
  const { book_uid, student_uid } = req.body;

  if (!book_uid)
    return res.status(400).json({ error: "book_uid required" });

  const record = issued.find(
    (r) =>
      r.book_uid.toLowerCase() === book_uid.toLowerCase() &&
      r.status === "Issued"
  );

  if (!record)
    return res.status(404).json({ error: "No active issue found" });

  record.status = "Returned";
  record.return_date = new Date().toISOString();
  saveIssued();

  console.log(`RETURNED: ${record.book_title}`);

  if (record.student_email) {
    const body =
      `Hello ${record.student_name},\n` +
      `Returned: ${record.book_title}`;
    await sendEmail(record.student_email, "Book Returned", body);
  }

  res.json({ ok: true, record });
});

app.get("/issued", (req, res) => res.json(issued));

// =====================================================
// 🧍 STUDENT ENTRY / EXIT
// =====================================================
app.post("/entry", (req, res) => {
  const { roll, name, dept } = req.body;

  if (!roll) return res.status(400).json({ error: "roll required" });

  const now = nowIST();
  const existing = entryLogs.find((r) => r.roll === roll && !r.exit_time);

  if (existing) {
    existing.exit_time = now;
    saveEntries();
    return res.json({ ok: true, message: "Exit recorded", record: existing });
  } else {
    const record = {
      roll,
      name: name || "Unknown",
      dept: dept || "Unknown",
      entry_time: now,
      exit_time: null,
    };
    entryLogs.push(record);
    saveEntries();
    return res.json({ ok: true, message: "Entry recorded", record });
  }
});

app.get("/logs", (req, res) => res.json(entryLogs));
app.get("/inside", (req, res) => res.json(entryLogs.filter((r) => !r.exit_time)));

// =====================================================
// 🚧 FIXED: GATE CHECK ENDPOINT
// =====================================================
// Accepts BOTH:
//    /check?uid=B001       ← NodeMCU uses this
//    /check?book_uid=B001  ← Web/API uses this
// =====================================================
app.get("/check", (req, res) => {
  const rawUID = req.query.uid || req.query.book_uid;

  if (!rawUID)
    return res.status(400).json({ ok: false, error: "UID required" });

  const book_uid = rawUID.trim().toLowerCase();

  // Normalize issued books list for comparison
  const record = issued.find(
    (r) => r.book_uid.toLowerCase() === book_uid && r.status === "Issued"
  );

  if (!record) {
    return res.status(404).json({
      ok: false,
      message: "Book NOT Issued"
    });
  }

  return res.json({
    ok: true,
    message: "Book Issued",
    record
  });
});

// =====================================================
// 🕘 CRON REMINDER
// =====================================================
const cronExpression = "0 9 * * *";
const reminderDaysBefore = parseInt(process.env.REMINDER_DAYS_BEFORE || "1", 10);

cron.schedule(cronExpression, async () => {
  for (const rec of issued) {
    if (rec.status !== "Issued") continue;

    const due = parseDate(rec.due_date);
    if (!due) continue;

    const diff = due.setHours(0, 0, 0, 0) - new Date().setHours(0, 0, 0, 0);
    const days = diff / 86400000;

    if (days === reminderDaysBefore && rec.student_email) {
      await sendEmail(
        rec.student_email,
        "Return Reminder",
        `Return: ${rec.book_title}`
      );
    }
  }
});

// =====================================================
// 🚀 STATIC + START SERVER
// =====================================================
app.use(express.static(path.join(__dirname, "public")));

app.listen(PORT, "0.0.0.0", () => {
  console.log(`Server running http://localhost:${PORT}`);
});
